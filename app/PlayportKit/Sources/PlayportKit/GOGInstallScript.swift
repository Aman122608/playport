// SPDX-License-Identifier: GPL-3.0-or-later
// A GOG build's install script, `goggame-<product>.script` in the game's folder (decision 0103), as
// GOG's own files give it: JSON with the product's id and `actions`, each named, with an `install`
// object (its `action` and `arguments`) and optional `languages` and `osBitness` that limit it.
// GOG Galaxy runs it after an install (its script interpreter, `__redist/ISI`, for a build whose
// manifest says `scriptInterpreter`). The actions seen in the owner's library:
//
//   setRegistry  root (HKLM, HKCU or their long forms), subkey (`\` or `/`), and a value:
//                valueName, valueData (`{app}` is the game's folder), valueType `string` or `dword`;
//                without a value it names a key, with `saveGameData` (cloud saves) or `deleteSubkeys`
//                (what the uninstall removes)
//   savePath     a save folder for GOG's cloud saves
//   setIni       a key in an INI file (filename with `{app}` or `{userappdata}`, section, keyName,
//                keyValue): BioShock's sets `[Engine.Engine] Language=int`, which its
//                `Builds/Release/Default.ini` already holds
//   installSDB   a Windows application-compatibility database (`goggame.sdb`)
//   Execute      a program from the game's or its support folder (NWN Diamond's and Spelunky's run
//                their `temp_executable`, decision 0099; Tomb Raider: Anniversary's a `.cmd` that
//                sets Windows Games Explorer keys)
//
// Playport writes the setRegistry values into the prefix before the runtime starts, as it writes a
// Steam install script's `Registry` (PrefixRegistryFile: a value the hive holds is kept); the
// other actions are left out and logged (setIni too: a changed game file would fail GOG's verify,
// and repair would put it back).

import Foundation

public struct GOGInstallScript: Equatable, Sendable {
    public struct Action: Equatable, Sendable {
        public var name: String
        /// `install.action`: `setRegistry`, `savePath`, `Execute`, …
        public var kind: String
        /// `install.arguments`, strings, and numbers and booleans as text (`true` as `1`); a list
        /// or object is left out.
        public var arguments: [String: String]
        /// Nil: every language.
        public var languages: [String]?
        /// Nil: both.
        public var osBitness: [String]?
    }

    public var productID: String
    public var actions: [Action]

    public static func parse(_ data: Data) throws -> GOGInstallScript {
        var bytes = data
        if bytes.starts(with: [0xEF, 0xBB, 0xBF]) { bytes = bytes.dropFirst(3) }
        guard let root = try? JSONSerialization.jsonObject(with: Data(bytes)) as? [String: Any],
              let actions = root["actions"] as? [[String: Any]] else {
            throw GOGInstallScriptError.notAScript
        }
        let product: String = (root["productId"] as? String) ?? (root["productId"] as? NSNumber)?.stringValue ?? ""
        return GOGInstallScript(productID: product, actions: actions.map { a in
            let install = a["install"] as? [String: Any] ?? [:]
            var args: [String: String] = [:]
            for (k, v) in install["arguments"] as? [String: Any] ?? [:] {
                if let s = v as? String { args[k] = s } else if let n = v as? NSNumber { args[k] = n.stringValue }
            }
            return Action(name: a["name"] as? String ?? "", kind: install["action"] as? String ?? "", arguments: args,
                          languages: a["languages"] as? [String], osBitness: a["osBitness"] as? [String])
        })
    }

    /// What a Play does with the script.
    public struct Plan: Equatable, Sendable {
        /// The setRegistry values, to write before the runtime starts.
        public var registry: [PrefixRegistryValue] = []
        /// The actions left out, for the log.
        public var notes: [String] = []
    }

    /// The script for a game in `folder` (its Windows path, `C:\Games\…`), installed in `language`
    /// with its 64-bit depots. An HKLM `Software` value is written in both registry views: GOG's
    /// script interpreter is a 32-bit program, so on Windows its values land in the 32-bit view
    /// (`Software\Wow6432Node`), where a 32-bit game reads them; the 64-bit view serves a 64-bit one.
    public func plan(folder: String, language: String = "en-US") -> Plan {
        var p = Plan()
        for a in actions {
            let label = "\(productID) \(a.name.isEmpty ? a.kind : a.name)"
            if let langs = a.languages, !langs.contains(where: { Self.language($0, matches: language) }) { continue }
            if let bits = a.osBitness, !bits.contains("64") { continue }
            switch a.kind {
            case "setRegistry":
                registry(a, folder: folder, label: label, into: &p)
            case "savePath":
                p.notes.append("\(label): savePath is for GOG's cloud saves; left out")
            case "setIni":
                p.notes.append("\(label): setIni \(a.arguments["filename"] ?? "") [\(a.arguments["section"] ?? "")] "
                               + "\(a.arguments["keyName"] ?? "")=\(a.arguments["keyValue"] ?? "") is not written; left out")
            case "installSDB":
                p.notes.append("\(label): installSDB (an application-compatibility database) has no Wine counterpart; left out")
            case "Execute":
                p.notes.append("\(label): Execute \(a.arguments["executable"] ?? "") is not run from the script; left out")
            default:
                p.notes.append("\(label): action \(a.kind.isEmpty ? "(none)" : a.kind) left out")
            }
        }
        return p
    }

    func registry(_ a: Action, folder: String, label: String, into p: inout Plan) {
        let subkey = (a.arguments["subkey"] ?? "").replacingOccurrences(of: "/", with: "\\")
            .split(separator: "\\").joined(separator: "\\")
        guard let name = a.arguments["valueName"] else {
            p.notes.append("\(label): registry key \(subkey) without a value; nothing to write")
            return
        }
        let raw = a.arguments["valueData"] ?? ""
        guard let expanded = Self.expand(raw, folder: folder) else {
            p.notes.append("\(label): \(subkey) \"\(name)\" names a folder Playport does not map (\(raw)); left out")
            return
        }
        let data: PrefixRegistryValue.Data
        switch (a.arguments["valueType"] ?? "string").lowercased() {
        case "string": data = .string(expanded)
        case "dword":
            guard let d = UInt32(expanded) ?? (expanded.lowercased().hasPrefix("0x") ? UInt32(expanded.dropFirst(2), radix: 16) : nil) else {
                p.notes.append("\(label): \(subkey) \"\(name)\" dword \(raw) is not a number; left out")
                return
            }
            data = .dword(d)
        case let other:
            p.notes.append("\(label): \(subkey) \"\(name)\" of type \(other) left out")
            return
        }
        guard let v = PrefixRegistryValue(windowsKey: (a.arguments["root"] ?? "") + "\\" + subkey, name: name, data: data) else {
            p.notes.append("\(label): registry root \(a.arguments["root"] ?? "") is not HKLM or HKCU; left out")
            return
        }
        p.registry.append(v)
        if let wow = Self.wow64View(v) { p.registry.append(wow) }
    }

    /// The 32-bit view of an HKLM `Software` value (`Software\Wow6432Node\…`); nil for one that is
    /// not redirected (HKCU, `Software\Classes`, a key already in the 32-bit view).
    static func wow64View(_ v: PrefixRegistryValue) -> PrefixRegistryValue? {
        let parts = v.key.split(separator: "\\").map(String.init)
        guard v.hive == .system, parts.count > 1, parts[0].lowercased() == "software",
              !["wow6432node", "classes"].contains(parts[1].lowercased()) else { return nil }
        var w = v
        w.key = (["Software", "Wow6432Node"] + parts.dropFirst()).joined(separator: "\\")
        return w
    }

    /// `{app}` and `{supportDir}` as the game's folder (Playport places the support files there,
    /// decision 0099), `/` as `\` after them; nil for another placeholder.
    static func expand(_ s: String, folder: String) -> String? {
        guard s.contains("{") else { return s }
        var out = s
        for token in ["{app}", "{supportDir}"] {
            out = out.replacingOccurrences(of: token, with: folder, options: .caseInsensitive)
        }
        guard !out.contains("{") else { return nil }
        return out.hasPrefix(folder) ? folder + out.dropFirst(folder.count).replacingOccurrences(of: "/", with: "\\") : out
    }

    /// An action's language `l` (`*`, `en-US`, or a bare `en`) covers the install's `language`.
    static func language(_ l: String, matches language: String) -> Bool {
        l == "*" || l.lowercased() == language.lowercased()
            || (!l.contains("-") && l.lowercased() == language.split(separator: "-").first?.lowercased())
    }

    /// The script files of a GOG install: `goggame-<product>.script` at its folder's root, from the
    /// install record's file list (one per installed product that ships one).
    public static func scriptFiles(_ paths: [String]) -> [String] {
        paths.filter { p in
            let l = p.lowercased()
            guard !l.contains("/"), l.hasPrefix("goggame-"), l.hasSuffix(".script") else { return false }
            return l.dropFirst("goggame-".count).dropLast(".script".count).allSatisfy(\.isNumber)
        }
    }
}

public enum GOGInstallScriptError: Error, Equatable {
    case notAScript
}

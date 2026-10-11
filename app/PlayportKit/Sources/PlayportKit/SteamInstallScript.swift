// SPDX-License-Identifier: GPL-3.0-or-later
// A Steam game's install scripts (`installscript.vdf`, decision 0081): the prerequisites Steam
// runs once before the first launch and the registry values it writes at install. The scripts
// come from the game's own depots (the files its manifests flag as install scripts) and from the
// Steamworks Common Redistributables depots it lists (CommonRedistributables). Each `Run Process`
// entry that applies to the prefix's Windows (64-bit Windows 10) becomes a Prerequisite, run from
// Play as decision 0079's are; its id is Steam's HasRunKey and the entry's name, the value Steam
// writes under that key after a success, and Playport writes it the same way.

import Foundation
import SteamClientKit

public struct SteamInstallScript: Equatable, Sendable {
    public struct RunProcess: Equatable, Sendable {
        public struct Step: Equatable, Sendable {
            public var process: String
            public var command: String
        }

        /// The entry's key (`x64 14.51.36247.0`): the value name Steam writes under HasRunKey.
        public var name: String
        public var hasRunKey: String?
        /// Steam skips the entry when HasRunKey holds a value at least this (1 when not given).
        public var minimumHasRunValue: UInt32
        /// `process 1` with `command 1`, `process 2`..., in order.
        public var steps: [Step]
        public var noCleanUp: Bool
        /// `Requirement_OS`, keys lower-cased.
        public var requirementOS: [String: String]
    }

    /// A `Registry` value: a Windows key path, its value name and data, `%INSTALLDIR%` left in.
    public struct RegistryEntry: Equatable, Sendable {
        public var key: String
        public var name: String
        public var data: PrefixRegistryValue.Data
    }

    public var runProcesses: [RunProcess]
    public var registry: [RegistryEntry]

    /// Parses a script (Valve KeyValues text; only its first root, `InstallScript` in any case, is
    /// read: the `kvsignatures` after it is Steam's). Entries Playport does not run (`Run Process On
    /// Uninstall`, `Firewall`, ...) are left out.
    public static func parse(_ data: Data) throws -> SteamInstallScript {
        let root = try KeyValue.parseText(Array(data))
        guard root.name.caseInsensitiveCompare("InstallScript") == .orderedSame else {
            throw SteamError.unsupported("not an install script (root \(root.name))")
        }
        var runs: [RunProcess] = []
        for section in root.children where section.name.caseInsensitiveCompare("Run Process") == .orderedSame {
            for e in section.children where e.value == nil {
                var steps: [RunProcess.Step] = []
                for n in 1...32 {
                    guard let p = e["process \(n)"]?.value, !p.isEmpty else { break }
                    steps.append(.init(process: p, command: e["command \(n)"]?.value ?? ""))
                }
                var req: [String: String] = [:]
                for r in e["Requirement_OS"]?.children ?? [] { req[r.name.lowercased()] = r.value ?? "" }
                runs.append(RunProcess(name: e.name, hasRunKey: e["HasRunKey"]?.value.flatMap { $0.isEmpty ? nil : $0 },
                                       minimumHasRunValue: e["MinimumHasRunValue"]?.uint32 ?? 1, steps: steps,
                                       noCleanUp: e["NoCleanUp"]?.value == "1", requirementOS: req))
            }
        }
        var registry: [RegistryEntry] = []
        for section in root.children where section.name.caseInsensitiveCompare("Registry") == .orderedSame {
            for key in section.children {
                for typed in key.children {
                    for v in typed.children {
                        guard let value = v.value else { continue }
                        switch typed.name.lowercased() {
                        case "string": registry.append(.init(key: key.name, name: v.name, data: .string(value)))
                        case "dword":
                            if let d = UInt32(value) ?? Int32(value).map({ UInt32(bitPattern: $0) }) {
                                registry.append(.init(key: key.name, name: v.name, data: .dword(d)))
                            }
                        default: break
                        }
                    }
                }
            }
        }
        return SteamInstallScript(runProcesses: runs, registry: registry)
    }
}

extension SteamInstallScript.RunProcess {
    /// Whether the entry runs on the prefix's Windows, which reports 64-bit Windows 10: an
    /// `Is64BitWindows` of 1 holds, an `OSType` holds only when it names Windows 10, and an
    /// unknown requirement does not hold (the entry is left out, not run against a guess).
    public var appliesToPrefix: Bool {
        requirementOS.allSatisfy { k, v in
            switch k {
            case "is64bitwindows": v == "1"
            case "ostype": v.lowercased() == "windows 10"
            default: false
            }
        }
    }

    /// The prerequisite id: Steam's HasRunKey and this entry's value under it; with no
    /// HasRunKey, the app, the script and the entry (Steam then runs it at every install).
    public func id(appID: UInt32, script: String) -> String {
        if let hasRunKey { return "\(hasRunKey)|\(name)" }
        return "steam-\(appID):\(script)|\(name)"
    }
}

/// What a game's install scripts ask for, ready to run.
public struct SteamPrerequisites: Equatable, Sendable {
    public var prerequisites: [Prerequisite] = []
    /// Registry values to write before the game (the scripts' `Registry` sections).
    public var registry: [PrefixRegistryValue] = []
    /// One line for each entry left out, and why.
    public var notes: [String] = []

    public init() {}

    /// A game's install scripts found by name, for a Steam game Playport did not download (no
    /// retained manifests flag them): `*installscript*.vdf` in its folder and in `_CommonRedist`
    /// below it (four levels), relative and `/`-separated, sorted.
    public static func scriptFiles(in dir: URL) -> [String] {
        func isScript(_ name: String) -> Bool {
            let n = name.lowercased()
            return n.hasSuffix(".vdf") && n.replacingOccurrences(of: "_", with: "").contains("installscript")
        }
        let fm = FileManager.default
        var out = ((try? fm.contentsOfDirectory(atPath: dir.path)) ?? []).filter(isScript)
        let redist = ((try? fm.contentsOfDirectory(atPath: dir.path)) ?? []).first { $0.lowercased() == "_commonredist" }
        if let redist, let e = fm.enumerator(atPath: dir.appendingPathComponent(redist).path) {
            while let rel = e.nextObject() as? String {
                if e.level > 4 { e.skipDescendants(); continue }
                if isScript((rel as NSString).lastPathComponent) { out.append(redist + "/" + rel) }
            }
        }
        return out.sorted()
    }

    /// Steam's exit codes for a success: 0, and 3010 (installed, restart needed), which Steam's
    /// own VC++ scripts map to 0.
    public static let successCodes: Set<UInt32> = [0, 3010]

    /// Adds one script's entries. `installDir` is the folder under games the script's
    /// `%INSTALLDIR%` names (the game's, or Steamworks Shared for a redistributable's); `script`
    /// its path there; `hive` the prefix's system.reg text, for Steam's HasRunKey check.
    public mutating func add(_ s: SteamInstallScript, appID: UInt32, installDir: String, script: String, games: URL,
                             hive: String) {
        let dos = "C:\\Games\\\(installDir)"
        func expand(_ v: String) -> String { replacingInstallDir(v, with: dos) }
        for r in s.runProcesses {
            let label = "\(script) \"\(r.name)\""
            guard r.appliesToPrefix else {
                notes.append("\(label): its Requirement_OS \(r.requirementOS) does not hold for 64-bit Windows 10; left out")
                continue
            }
            let id = r.id(appID: appID, script: script)
            if prerequisites.contains(where: { $0.ids.first == id }) {
                notes.append("\(label): \(id) is listed already; left out")
                continue
            }
            if let key = r.hasRunKey, let v = PrefixRegistryValue(windowsKey: key, name: r.name, data: .dword(1)),
               let have = PrefixRegistryFile.dword(inHive: hive, key: v.key, name: r.name), have >= r.minimumHasRunValue {
                notes.append("\(label): \(key) holds \"\(r.name)\"=\(have) in the prefix; already run")
                continue
            }
            var made: [Prerequisite] = []
            for (n, step) in r.steps.enumerated() {
                guard let rest = Self.insideInstallDir(step.process) else {
                    notes.append("\(label): process \(n + 1) \(step.process) is not under %INSTALLDIR%; left out")
                    made = []
                    break
                }
                // As the guest sees it: each component matched without case.
                guard let onDisk = Adoption.locate(rest, in: games.appendingPathComponent(installDir, isDirectory: true)) else {
                    notes.append("\(label): \(rest) is not in Games/\(installDir); left out")
                    made = []
                    break
                }
                guard let plan = try? LaunchPlan.make(installDir: installDir, executable: onDisk, arguments: []) else {
                    notes.append("\(label): process \(n + 1) \(step.process) is not a path in the folder; left out")
                    made = []
                    break
                }
                let stepID = r.steps.count == 1 ? id : "\(id) step \(n + 1)"
                made.append(Prerequisite(ids: [stepID], name: "Steam \(r.name) (\(script))", exe: plan.exe,
                                         args: Prerequisite.splitArguments(expand(step.command))))
            }
            guard !made.isEmpty else {
                if r.steps.isEmpty { notes.append("\(label): no process 1; left out") }
                continue
            }
            if let key = r.hasRunKey, let v = PrefixRegistryValue(windowsKey: key, name: r.name, data: .dword(1)) {
                made[made.count - 1].successRegistry = [v]
            }
            prerequisites += made
        }
        for e in s.registry {
            guard let v = PrefixRegistryValue(windowsKey: expand(e.key), name: expand(e.name), data: {
                if case .string(let t) = e.data { return .string(expand(t)) }
                return e.data
            }()) else {
                notes.append("\(script): registry key \(e.key) is not under HKEY_LOCAL_MACHINE or HKEY_CURRENT_USER; left out")
                continue
            }
            registry.append(v)
        }
    }

    /// The rest of a path after a leading `%INSTALLDIR%` and its separator; nil otherwise.
    static func insideInstallDir(_ path: String) -> String? {
        let token = "%INSTALLDIR%"
        guard path.count > token.count, path.prefix(token.count).uppercased() == token else { return nil }
        let rest = path.dropFirst(token.count).drop { $0 == "\\" || $0 == "/" }
        return rest.isEmpty ? nil : String(rest)
    }

    func replacingInstallDir(_ s: String, with dos: String) -> String {
        s.replacingOccurrences(of: "%INSTALLDIR%", with: dos, options: .caseInsensitive)
    }
}

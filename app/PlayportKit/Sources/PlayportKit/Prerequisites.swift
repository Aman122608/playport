// SPDX-License-Identifier: GPL-3.0-or-later
// A title's own prerequisite installer, run once from Play (decision 0079):
// the copy the title ships (an Epic build's PrereqPath and PrereqArgs, for its
// PrereqIds; Unreal Engine's UEPrereqSetup_x64.exe, which a UE game's bootstrap
// runs itself on Windows when the VC++ runtime's key is missing, a key the
// prefix seed has), started before the game in the same session and waited for.
// Playport never ships or downloads a redistributable. Each id is run once per
// prefix, whatever the exit code: the record (PrerequisiteRecord, a file in the
// prefix, so it goes with the prefix) keeps the code, and the game starts
// either way, as Epic's launcher does.

import Foundation
import SteamClientKit

public struct Prerequisite: Equatable, Sendable {
    /// What the run installs, as the store names it (Epic's PrereqIds): the
    /// record's keys. It runs when one of them is not recorded.
    public var ids: [String]
    /// The store's name for it (Epic's PrereqName), for the log.
    public var name: String
    /// Relative to C:\, backslash-separated, inside C:\Games (LaunchPlan.exe).
    public var exe: String
    public var args: [String]

    public init(ids: [String], name: String, exe: String, args: [String]) {
        self.ids = ids
        self.name = name
        self.exe = exe
        self.args = args
    }

    /// The prerequisite a build names, its path relative to the game's folder and
    /// its arguments as one command-line string; nil when it names none (no ids
    /// or no path). Throws for a path outside the folder (LaunchPlanError.badPath).
    public static func make(ids: [String], name: String, installDir: String, path: String,
                            arguments: String) throws -> Prerequisite? {
        let ids = ids.filter { !$0.isEmpty }
        guard !ids.isEmpty, !path.isEmpty else { return nil }
        let plan = try LaunchPlan.make(installDir: installDir, executable: path, arguments: [])
        let file = plan.exe.split(separator: "\\").last.map(String.init) ?? path
        return Prerequisite(ids: ids, name: name.isEmpty ? file : name,
                            exe: plan.exe, args: splitArguments(arguments))
    }

    /// Where an Unreal Engine 4 or 5 game ships the engine's prerequisite installer
    /// (the VC++ 2015-22 runtimes, x64 and x86, and DirectX's), relative to its folder.
    public static let unrealEnginePath = "Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe"

    /// The Unreal Engine prerequisite installer the game in `installDir` under
    /// `games` ships, with the arguments its bootstrap would not need a person
    /// for (`/quiet /norestart`); nil when it ships none. Its id is the file's
    /// size and SHA-256 prefix, so a newer installer runs again. Reads the whole
    /// file (about 50 MB): not on the main thread.
    public static func unrealEngine(installDir: String, games: URL) -> Prerequisite? {
        guard let plan = try? LaunchPlan.make(installDir: installDir, executable: unrealEnginePath, arguments: []) else { return nil }
        let url = games.appendingPathComponent(installDir, isDirectory: true).appendingPathComponent(unrealEnginePath)
        guard let size = InstallFS.fileSize(url), let digest = InstallFS.sha256(url) else { return nil }
        let hex = digest.prefix(8).map { String(format: "%02x", $0) }.joined()
        return Prerequisite(ids: ["UEPrereqSetup_x64:\(size)-\(hex)"], name: "Unreal Engine prerequisites", exe: plan.exe,
                            args: ["/quiet", "/norestart"])
    }

    /// The title's prerequisites to offer a Play: the store's, then the Unreal
    /// Engine installer unless the store's names the same file.
    public static func merge(store: [Prerequisite], unrealEngine: Prerequisite?) -> [Prerequisite] {
        guard let ue = unrealEngine, !store.contains(where: { $0.exe.lowercased() == ue.exe.lowercased() }) else { return store }
        return store + [ue]
    }

    /// A command-line string's arguments, as CommandLineToArgvW reads the
    /// common cases: split at spaces and tabs outside double quotes, quotes removed.
    public static func splitArguments(_ line: String) -> [String] {
        var args: [String] = [], current = "", quoted = false, any = false
        for c in line {
            if c == "\"" {
                quoted.toggle()
                any = true
            } else if (c == " " || c == "\t") && !quoted {
                if any { args.append(current) }
                current = ""
                any = false
            } else {
                current.append(c)
                any = true
            }
        }
        if any { args.append(current) }
        return args
    }
}

/// Which prerequisite ids ran in the prefix, with their exit codes.
public struct PrerequisiteRecord: Codable, Equatable, Sendable {
    public struct Run: Codable, Equatable, Sendable {
        public var name: String
        public var exitCode: UInt32
        public var date: Date
        /// The title whose Play ran it.
        public var title: String

        public init(name: String, exitCode: UInt32, date: Date, title: String) {
            self.name = name
            self.exitCode = exitCode
            self.date = date
            self.title = title
        }
    }

    /// By id.
    public var runs: [String: Run] = [:]

    public init(runs: [String: Run] = [:]) {
        self.runs = runs
    }

    /// The prerequisite's ids that have not run in this prefix.
    public func pending(_ p: Prerequisite) -> [String] { p.ids.filter { runs[$0] == nil } }

    /// Every id of the prerequisite ran, with this exit code.
    public mutating func record(_ p: Prerequisite, exitCode: UInt32, title: String, date: Date = Date()) {
        for id in p.ids { runs[id] = Run(name: p.name, exitCode: exitCode, date: date, title: title) }
    }
}

/// The record's file. An absent or unreadable file is an empty record: the
/// prerequisites then run again, which their installers allow.
public struct PrerequisiteStore: Sendable {
    /// In the prefix (beside its registry hives), so a new prefix runs them again.
    public static let fileName = "playport-prerequisites.json"

    public let url: URL

    public init(url: URL) {
        self.url = url
    }

    public init(prefix: URL) {
        self.init(url: prefix.appendingPathComponent(Self.fileName))
    }

    public func load() -> PrerequisiteRecord {
        (try? Data(contentsOf: url)).flatMap { try? CatalogStore.decoder.decode(PrerequisiteRecord.self, from: $0) }
            ?? PrerequisiteRecord()
    }

    public func save(_ record: PrerequisiteRecord) throws {
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try CatalogStore.encoder.encode(record).write(to: url, options: .atomic)
    }
}

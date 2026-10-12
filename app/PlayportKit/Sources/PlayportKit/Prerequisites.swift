// SPDX-License-Identifier: GPL-3.0-or-later
// A title's own prerequisite installer, run once from Play (decision 0079):
// the copy the title ships (an Epic build's PrereqPath and PrereqArgs, for its
// PrereqIds; Unreal Engine's UEPrereqSetup_x64.exe, which a UE game's bootstrap
// runs itself on Windows when the VC++ runtime's key is missing, a key the
// prefix seed has), started before the game in the same session and waited for.
// A Steam game's install-script entries run the same way (SteamInstallScript, decision
// 0081), and a GOG game's redistributables (GOGDependencies, decision 0090); Playport ships no
// redistributable and downloads only the store's ones a game lists. Each id is run once per
// prefix: the record (PrerequisiteRecord, a file in the prefix, so it goes with the prefix)
// keeps the code and the app build that ran it, and the game starts either way, as Epic's
// launcher does. A failure runs once more with each new app build (PLA-171).

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
    /// Written to the prefix's registry after it ends with one of `successCodes`
    /// (Steam's HasRunKey value, decision 0081); empty for the other stores'.
    public var successRegistry: [PrefixRegistryValue] = []

    public init(ids: [String], name: String, exe: String, args: [String], successRegistry: [PrefixRegistryValue] = []) {
        self.ids = ids
        self.name = name
        self.exe = exe
        self.args = args
        self.successRegistry = successRegistry
    }

    /// The exit codes that count as a success: 0, and 3010 (installed, restart needed), which
    /// Steam's own VC++ scripts map to 0.
    public static let successCodes: Set<UInt32> = [0, 3010]

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

/// Which prerequisite ids ran in the prefix, with their exit codes and the app builds that ran them.
public struct PrerequisiteRecord: Codable, Equatable, Sendable {
    public struct Run: Codable, Equatable, Sendable {
        public var name: String
        /// Nil for an entry Playport skipped (`skipped`): it did not run.
        public var exitCode: UInt32?
        public var date: Date
        /// The title whose Play ran it.
        public var title: String
        /// The app build that ran it (the app's own identifier); nil in a record written before
        /// builds were kept, which counts as another build.
        public var build: String?
        /// Why Playport did not run it (SteamPrerequisites.Skipped); nil when it ran.
        public var skipped: String?

        public init(name: String, exitCode: UInt32?, date: Date, title: String, build: String? = nil, skipped: String? = nil) {
            self.name = name
            self.exitCode = exitCode
            self.date = date
            self.title = title
            self.build = build
            self.skipped = skipped
        }

        public var succeeded: Bool { exitCode.map { Prerequisite.successCodes.contains($0) } ?? false }
        /// The record's outcome for a log: `exit 0x…`, or `skipped`.
        public var outcome: String { exitCode.map { "exit 0x\(String($0, radix: 16))" } ?? "skipped" }
    }

    /// By id.
    public var runs: [String: Run] = [:]

    public init(runs: [String: Run] = [:]) {
        self.runs = runs
    }

    /// The prerequisite's ids to run with app build `build`: those not run in this prefix, and
    /// those whose run failed with another build (PLA-171), since a new build may have fixed what
    /// failed (a missing program, a runtime fault). A failure this build recorded stays done, so
    /// each failure runs once more per new build, not at every Play.
    public func pending(_ p: Prerequisite, build: String) -> [String] {
        p.ids.filter { id in
            guard let run = runs[id] else { return true }
            return run.skipped == nil && !run.succeeded && run.build != build
        }
    }

    /// `ids` were skipped, not run, for `reason` (a recorded failure among them included), under
    /// app build `build`: they are not pending again with any build.
    public mutating func recordSkipped(ids: [String], name: String, reason: String, title: String, build: String,
                                       date: Date = Date()) {
        for id in ids { runs[id] = Run(name: name, exitCode: nil, date: date, title: title, build: build, skipped: reason) }
    }

    /// Every id of the prerequisite ran, with this exit code, under app build `build`.
    public mutating func record(_ p: Prerequisite, exitCode: UInt32, title: String, build: String, date: Date = Date()) {
        for id in p.ids { runs[id] = Run(name: p.name, exitCode: exitCode, date: date, title: title, build: build) }
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

/// The Play that goes on in a new app process once its prerequisites ran (decision 0084). Each
/// process a prerequisite starts (cmd, conhost, start, an installer and its own children) leaves
/// its image copies in the JIT pool's head, which nothing returns, and a process plays in one
/// session (decision 0030); so when they left the game too little room, the game starts after
/// Playport's restart, in a fresh session with a fresh pool, as the same Play (decision 0094).
/// The restart record carries this (AppRestart's `restart.json`).
public struct PlayContinuation: Codable, Equatable, Sendable {
    /// The catalogue's id of the title, as the library's Play takes it.
    public var titleID: String
    /// Its name, for the screen shown across the restart.
    public var title: String
    /// Its Steam app, for that screen's art; nil for another store's.
    public var appID: UInt32?
    /// An Epic game the player chose to play offline (not signed in) plays offline again.
    public var epicOffline: Bool

    public init(titleID: String, title: String, appID: UInt32? = nil, epicOffline: Bool = false) {
        self.titleID = titleID
        self.title = title
        self.appID = appID
        self.epicOffline = epicOffline
    }

    /// Whether the launch restarts Playport before the game: one prerequisite at least ran
    /// (started and ended) in this session, Playport can restart itself, and the pool after them
    /// leaves the game less than `JitPool.gameRoomMB` (`Use.roomForGame`), or ran out, or could
    /// not be read (decision 0094). Otherwise the game starts in the same session. Without the
    /// restart (inside LiveContainer, or JIT from another app with no pairing) it always does,
    /// as before decision 0084.
    public static func restartsBeforeGame(prerequisitesRan: Int, canRestart: Bool, pool: JitPool.Use?) -> Bool {
        guard prerequisitesRan > 0, canRestart else { return false }
        guard let pool, pool.exhaustion == nil else { return true }
        return pool.roomForGame < UInt64(JitPool.gameRoomMB) << 20
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import XCTest
@testable import PlayportKit

final class PrerequisitesTests: XCTestCase {
    func testABuildsPrerequisiteIsAFileInsideItsFolder() throws {
        let p = try Prerequisite.make(ids: ["a", "b"], name: "UE Prerequisites (x64)", installDir: "HouseOfGolf2",
                                      path: "Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe", arguments: "/quiet /norestart")
        XCTAssertEqual(p, Prerequisite(ids: ["a", "b"], name: "UE Prerequisites (x64)",
                                       exe: #"Games\HouseOfGolf2\Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe"#,
                                       args: ["/quiet", "/norestart"]))
        // None named: no ids or no path.
        XCTAssertNil(try Prerequisite.make(ids: [], name: "", installDir: "G", path: "Setup.exe", arguments: ""))
        XCTAssertNil(try Prerequisite.make(ids: [""], name: "", installDir: "G", path: "Setup.exe", arguments: ""))
        XCTAssertNil(try Prerequisite.make(ids: ["a"], name: "", installDir: "G", path: "", arguments: ""))
        // Outside the folder: refused.
        XCTAssertThrowsError(try Prerequisite.make(ids: ["a"], name: "", installDir: "G", path: "../Setup.exe", arguments: ""))
        // No name: the file's.
        XCTAssertEqual(try Prerequisite.make(ids: ["a"], name: "", installDir: "G", path: #"Redist\Setup.exe"#, arguments: "")?.name,
                       "Setup.exe")
    }

    func testAnUnrealEngineGamesOwnInstallerIsFoundAndNamedByItsContent() throws {
        let games = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: games) }
        XCTAssertNil(Prerequisite.unrealEngine(installDir: "Golf", games: games), "none shipped")
        let file = games.appendingPathComponent("Golf").appendingPathComponent(Prerequisite.unrealEnginePath)
        try FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data("abc".utf8).write(to: file)
        let ue = try XCTUnwrap(Prerequisite.unrealEngine(installDir: "Golf", games: games))
        // SHA-256("abc") begins ba7816bf8f01cfea.
        XCTAssertEqual(ue, Prerequisite(ids: ["UEPrereqSetup_x64:3-ba7816bf8f01cfea"], name: "Unreal Engine prerequisites",
                                        exe: #"Games\Golf\Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe"#,
                                        args: ["/quiet", "/norestart"]))
        try Data("abcd".utf8).write(to: file)
        XCTAssertNotEqual(Prerequisite.unrealEngine(installDir: "Golf", games: games)?.ids, ue.ids, "a newer installer runs again")

        // A store prerequisite naming the same file runs once, as the store's.
        let epic = try XCTUnwrap(try Prerequisite.make(ids: ["e"], name: "Epic's", installDir: "Golf",
                                                       path: "engine/extras/redist/en-us/UEPrereqSetup_x64.exe", arguments: "/quiet"))
        XCTAssertEqual(Prerequisite.merge(store: [epic], unrealEngine: ue), [epic])
        let other = Prerequisite(ids: ["o"], name: "Other", exe: #"Games\Golf\Setup.exe"#, args: [])
        XCTAssertEqual(Prerequisite.merge(store: [other], unrealEngine: ue), [other, ue])
        XCTAssertEqual(Prerequisite.merge(store: [], unrealEngine: nil), [])
    }

    func testArgumentsSplitAsACommandLine() {
        XCTAssertEqual(Prerequisite.splitArguments("/quiet /norestart"), ["/quiet", "/norestart"])
        XCTAssertEqual(Prerequisite.splitArguments("  -a\t\"b c\"  d\"e f\" \"\" "), ["-a", "b c", "de f", ""])
        XCTAssertEqual(Prerequisite.splitArguments(""), [])
    }

    func testEachIdRunsOncePerPrefixAndBuild() throws {
        let p = Prerequisite(ids: ["a", "b"], name: "Setup", exe: #"Games\G\Setup.exe"#, args: [])
        var record = PrerequisiteRecord()
        XCTAssertEqual(record.pending(p, build: "b1"), ["a", "b"])
        let at = Date(timeIntervalSince1970: 1_790_000_000)
        record.record(p, exitCode: 0x666, title: "G", build: "b1", date: at)
        XCTAssertEqual(record.pending(p, build: "b1"), [], "a failure this build recorded is not run again")
        XCTAssertEqual(record.runs["b"], .init(name: "Setup", exitCode: 0x666, date: at, title: "G", build: "b1"))
        // A later title build with one more id runs again.
        XCTAssertEqual(record.pending(Prerequisite(ids: ["a", "c"], name: "Setup", exe: p.exe, args: []), build: "b1"), ["c"])

        let dir = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: dir) }
        let store = PrerequisiteStore(prefix: dir)
        XCTAssertEqual(store.url.lastPathComponent, "playport-prerequisites.json")
        XCTAssertEqual(store.load(), PrerequisiteRecord())
        try store.save(record)
        XCTAssertEqual(store.load(), record)
        try Data("not json".utf8).write(to: store.url)
        XCTAssertEqual(store.load(), PrerequisiteRecord(), "an unreadable record runs them again")
    }

    func testAFailureRunsOnceMoreWithANewAppBuild() {
        let p = Prerequisite(ids: ["x86", "x64"], name: "VC++", exe: #"Games\G\vc.cmd"#, args: [])
        var record = PrerequisiteRecord()
        record.record(p, exitCode: 1, title: "G", build: "old")
        XCTAssertEqual(record.pending(p, build: "old"), [])
        XCTAssertEqual(record.pending(p, build: "new"), ["x86", "x64"], "a new build runs a failure again")
        // Its new record carries the new build: it is not run a third time by that build.
        record.record(p, exitCode: 1, title: "G", build: "new")
        XCTAssertEqual(record.runs["x64"]?.build, "new")
        XCTAssertEqual(record.pending(p, build: "new"), [])
        XCTAssertEqual(record.pending(p, build: "newer"), ["x86", "x64"])
    }

    func testASuccessIsNeverRunAgain() {
        let p = Prerequisite(ids: ["a"], name: "Setup", exe: #"Games\G\Setup.exe"#, args: [])
        for code: UInt32 in [0, 3010] {
            var record = PrerequisiteRecord()
            record.record(p, exitCode: code, title: "G", build: "old")
            XCTAssertTrue(record.runs["a"]?.succeeded == true)
            XCTAssertEqual(record.pending(p, build: "new"), [], "exit \(code) is a success")
        }
    }

    func testARecordFromBeforeBuildsWereKeptRerunsItsFailuresOnce() throws {
        // As the PLA-158 builds wrote it: no build field.
        let json = """
        {"runs":{"fail":{"name":"DX","exitCode":4294967287,"date":"2026-10-11T09:58:00Z","title":"CC"},
                 "ok":{"name":"VC","exitCode":0,"date":"2026-10-11T09:58:00Z","title":"CC"}}}
        """
        var record = try CatalogStore.decoder.decode(PrerequisiteRecord.self, from: Data(json.utf8))
        XCTAssertNil(record.runs["fail"]?.build)
        let fail = Prerequisite(ids: ["fail"], name: "DX", exe: #"Games\G\dx.exe"#, args: [])
        let ok = Prerequisite(ids: ["ok"], name: "VC", exe: #"Games\G\vc.exe"#, args: [])
        XCTAssertEqual(record.pending(fail, build: "b2"), ["fail"])
        XCTAssertEqual(record.pending(ok, build: "b2"), [])
        record.record(fail, exitCode: 0xfffffff7, title: "CC", build: "b2")
        XCTAssertEqual(record.pending(fail, build: "b2"), [])
    }

    func testASkippedEntryIsRecordedWithoutAnExitCodeAndNeverRunsAgain() throws {
        // Castle Crashers' DXSETUP as the PLA-158 builds recorded it: a failure, which 0083 reran per build.
        let json = """
        {"runs":{"dx":{"name":"DX","exitCode":3221225496,"date":"2026-10-11T09:58:00Z","title":"CC","build":"b1"}}}
        """
        var record = try CatalogStore.decoder.decode(PrerequisiteRecord.self, from: Data(json.utf8))
        let dx = Prerequisite(ids: ["dx"], name: "DX", exe: #"Games\G\DXSETUP.exe"#, args: [])
        XCTAssertEqual(record.pending(dx, build: "b2"), ["dx"])
        let at = Date(timeIntervalSince1970: 1_790_000_000)
        record.recordSkipped(ids: ["dx"], name: "Steam DirectX", reason: "covered", title: "CC", build: "b2", date: at)
        XCTAssertEqual(record.runs["dx"], .init(name: "Steam DirectX", exitCode: nil, date: at, title: "CC", build: "b2", skipped: "covered"))
        XCTAssertFalse(record.runs["dx"]?.succeeded ?? true, "a skip is not a success with an exit code")
        XCTAssertEqual(record.runs["dx"]?.outcome, "skipped")
        XCTAssertEqual(record.pending(dx, build: "b2"), [])
        XCTAssertEqual(record.pending(dx, build: "b3"), [], "the failure it replaced does not rerun with a new build")

        let dir = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: dir) }
        let store = PrerequisiteStore(prefix: dir)
        try store.save(record)
        XCTAssertEqual(store.load(), record)
        let text = try String(contentsOf: store.url, encoding: .utf8)
        XCTAssertTrue(text.contains("\"skipped\" : \"covered\"") || text.contains("\"skipped\":\"covered\""), text)
        XCTAssertFalse(text.contains("exitCode"), text)
    }

    func testAPlayRestartsBeforeItsGameOnlyAfterAPrerequisiteRan() {
        XCTAssertTrue(PlayContinuation.restartsBeforeGame(prerequisitesRan: 1, canRestart: true))
        XCTAssertTrue(PlayContinuation.restartsBeforeGame(prerequisitesRan: 2, canRestart: true))
        // None ran (all recorded, none listed, or none could start): the game starts in this session.
        XCTAssertFalse(PlayContinuation.restartsBeforeGame(prerequisitesRan: 0, canRestart: true))
        // No restart possible (LiveContainer, JIT from another app): the game starts in this session.
        XCTAssertFalse(PlayContinuation.restartsBeforeGame(prerequisitesRan: 1, canRestart: false))
    }

    func testAPlayContinuationRoundTripsAndDecodesWithDefaults() throws {
        let c = PlayContinuation(titleID: "app-10", title: "Counter-Strike", appID: 10)
        XCTAssertEqual(try JSONDecoder().decode(PlayContinuation.self, from: JSONEncoder().encode(c)), c)
        XCTAssertFalse(c.epicOffline)
        let epic = PlayContinuation(titleID: "epic-x", title: "X", epicOffline: true)
        XCTAssertEqual(try JSONDecoder().decode(PlayContinuation.self, from: JSONEncoder().encode(epic)), epic)
        XCTAssertNil(epic.appID)
    }
}

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

    func testEachIdRunsOncePerPrefixWhateverItsExitCode() throws {
        let p = Prerequisite(ids: ["a", "b"], name: "Setup", exe: #"Games\G\Setup.exe"#, args: [])
        var record = PrerequisiteRecord()
        XCTAssertEqual(record.pending(p), ["a", "b"])
        let at = Date(timeIntervalSince1970: 1_790_000_000)
        record.record(p, exitCode: 0x666, title: "G", date: at)
        XCTAssertEqual(record.pending(p), [])
        XCTAssertEqual(record.runs["b"], .init(name: "Setup", exitCode: 0x666, date: at, title: "G"))
        // A later build with one more id runs again.
        XCTAssertEqual(record.pending(Prerequisite(ids: ["a", "c"], name: "Setup", exe: p.exe, args: [])), ["c"])

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
}

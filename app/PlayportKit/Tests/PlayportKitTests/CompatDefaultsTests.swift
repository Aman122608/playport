// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import XCTest
@testable import PlayportKit

final class CompatDefaultsTests: XCTestCase {
    /// The table the app ships (app/PlayportKit/Titles).
    static let shipped = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
        .appendingPathComponent("../../Titles", isDirectory: true).standardizedFileURL

    func parse(_ json: String) throws -> CompatDefaults { try CompatDefaults.parse(Data(json.utf8)) }

    func testTheShippedTableReadsAndGivesSuperMeatBoy16x9() throws {
        let table = try CompatDefaults.load(directory: Self.shipped)
        let smb = try XCTUnwrap(table.entry(for: "epic-Buffalo"))
        XCTAssertEqual(smb.settings, LaunchSettings(screen: "1280x720"))
        XCTAssertEqual(ScreenAspect.of(smb.settings.screen), .wide16x9)
        XCTAssertNil(table.entry(for: "epic-buffalo"))   // ids are case-sensitive
        XCTAssertNil(table.entry(for: "app-367520"))
    }

    /// The table reaches players: no issue id or tracker address anywhere in it.
    func testTheShippedTableNamesNothingInternal() throws {
        let text = try String(contentsOf: Self.shipped.appendingPathComponent(CompatDefaults.fileName), encoding: .utf8)
        XCTAssertFalse(CompatDefaults.containsInternalReference(text))
        XCTAssertFalse(text.contains("PLA-"))
        XCTAssertTrue(CompatDefaults.containsInternalReference("see pla-108"))
        XCTAssertTrue(CompatDefaults.containsInternalReference("https://linear.app/x"))
        XCTAssertFalse(CompatDefaults.containsInternalReference("This game draws nothing on a screen wider than 16:9."))
    }

    func testTheTableRefusesWhatItDoesNotKnow() {
        let entry = #""settings":{"screen":"1280x720"},"reason":"Why.""#
        let bad: [(String, CompatDefaultsError)] = [
            (#"[]"#, .notAnObject("the file")),
            (#"{"titles":{},"comment":"x"}"#, .unknownKey("comment")),
            (#"{"titles":{"Epic-Buffalo":{\#(entry)}}}"#, .badTitle("Epic-Buffalo")),
            (#"{"titles":{"epic-A":{\#(entry),"ref":"x"}}}"#, .unknownKey("epic-A.ref")),
            (#"{"titles":{"epic-A":{"settings":{"screen":"1280x720"}}}}"#, .noReason("epic-A")),
            (#"{"titles":{"epic-A":{"settings":{"screen":"1280x720"},"reason":"PLA-108"}}}"#, .internalReference("epic-A")),
            (#"{"titles":{"epic-A":{"settings":{"aspect":"16:9"},"reason":"Why."}}}"#, .badSetting("epic-A.aspect")),
            (#"{"titles":{"epic-A":{"settings":{"graphics":"metal"},"reason":"Why."}}}"#, .badSetting("epic-A.graphics")),
            (#"{"titles":{"epic-A":{"settings":{"screen":"wide"},"reason":"Why."}}}"#, .badSetting("epic-A.screen")),
            (#"{"titles":{"epic-A":{"settings":{"frameLimit":-1},"reason":"Why."}}}"#, .badSetting("epic-A.frameLimit")),
            (#"{"titles":{"epic-A":{"settings":{},"reason":"Why."}}}"#, .notAnObject("epic-A.settings")),
        ]
        for (json, error) in bad {
            XCTAssertThrowsError(try parse(json), json) { XCTAssertEqual($0 as? CompatDefaultsError, error, json) }
        }
        // Any launch setting may be a default, in the --settings JSON form.
        XCTAssertEqual(try parse(#"{"titles":{"app-1":{"settings":{"frameLimit":30,"graphics":"vulkan","arguments":"-dx9"},"reason":"Why."}}}"#)
            .entry(for: "app-1")?.settings, LaunchSettings(frameLimit: 30, graphics: .vulkan, arguments: "-dx9"))
    }

    /// The person's own choice, then the compatibility default, then Settings, then Playport's.
    func testPrecedenceAndReset() {
        let compat = LaunchSettings(screen: "1280x720", frameLimit: 30)
        let global = LaunchSettings(screen: "1080", frameLimit: 40, graphics: .vulkan)
        func effective(_ own: LaunchSettings?) -> LaunchSettings.Effective {
            LaunchSettings.resolve(game: own.map { $0.over(compat) } ?? compat, global: global)
        }
        XCTAssertEqual(effective(nil).screen, "1280x720")                                   // compat over Settings
        XCTAssertEqual(effective(nil).frameLimit, 30)
        XCTAssertEqual(effective(nil).graphics, .vulkan)                                   // Settings where compat is silent
        XCTAssertEqual(effective(LaunchSettings(screen: "720")).screen, "720")              // the person's own wins
        XCTAssertEqual(effective(LaunchSettings(screen: "720")).frameLimit, 30)
        // Reset (the row's value back to nil) returns to the compatibility default, not Settings'.
        var own = LaunchSettings(screen: "720")
        own.screen = nil
        XCTAssertEqual(effective(own).screen, "1280x720")
        XCTAssertEqual(LaunchSettings.resolve(game: LaunchSettings().over(nil), global: LaunchSettings()).screen, "720")
        XCTAssertEqual(LaunchSettings(arguments: "-a").over(LaunchSettings(arguments: "-b")).arguments, "-a")
        XCTAssertEqual(LaunchSettings().over(LaunchSettings(arguments: "-b", runtime: "vram-mb=1")),
                       LaunchSettings(arguments: "-b", runtime: "vram-mb=1"))
    }

    /// The marker shows only while the compatibility value is the effective one.
    func testTheMarkerShowsOnlyForAnEffectiveCompatibilityValue() {
        let name: (String?) -> String = { $0 ?? "-" }
        let compat = OptionValue.of(own: nil, compat: Optional("1280x720"), inherited: "720", name: name)
        XCTAssertTrue(compat.compat)
        XCTAssertFalse(compat.changed)
        XCTAssertEqual(compat.text, "\(OptionValue.compatMarker) For this game · 1280x720")
        let own = OptionValue.of(own: Optional("720"), compat: Optional("1280x720"), inherited: "720", name: name)
        XCTAssertFalse(own.compat)
        XCTAssertTrue(own.changed)
        let none = OptionValue.of(own: String??.none, inherited: "720", name: name)
        XCTAssertFalse(none.compat)
        XCTAssertEqual(none.text, "Default · 720")
    }
}

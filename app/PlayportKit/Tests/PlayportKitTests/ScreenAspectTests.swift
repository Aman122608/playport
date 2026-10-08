// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import XCTest
@testable import PlayportKit

final class ScreenAspectTests: XCTestCase {
    func testEveryShapeOffersTheSameResolutions() {
        XCTAssertEqual(LaunchSettings.screenPresets, ["native", "1080", "900", "720", "540"])
        XCTAssertEqual(ScreenAspect.allCases.map(\.rawValue), ["phone", "16:9", "16:10", "4:3"])
        XCTAssertTrue(LaunchSettings.screenPresets.allSatisfy(LaunchPlan.validScreen))
    }

    /// A level becomes the guest spec HostIOKit Display.guestSize computes for the shape.
    func testALevelInAShapeIsItsGuestSpec() {
        XCTAssertEqual(ScreenAspect.phone.spec(level: "720"), "720")
        XCTAssertNil(ScreenAspect.phone.spec(level: nil))
        XCTAssertEqual(ScreenAspect.wide16x9.spec(level: "720"), "16:9@720")
        XCTAssertEqual(ScreenAspect.wide16x10.spec(level: "1080"), "16:10@1080")
        XCTAssertEqual(ScreenAspect.standard4x3.spec(level: "native"), "4:3@native")
        XCTAssertEqual(ScreenAspect.standard4x3.spec(level: "NATIVE"), "4:3@native")
        XCTAssertEqual(ScreenAspect.wide16x9.spec(level: nil), "16:9@native")
        // A size set before stays that size whatever the shape.
        XCTAssertEqual(ScreenAspect.wide16x9.spec(level: "1024x768"), "1024x768")
        XCTAssertEqual(ScreenAspect.wide16x9.spec(level: "4:3"), "4:3")
    }

    /// A screen saved before the aspect ratio choice keeps its meaning: the phone's shape.
    func testSavedScreensFromBeforeAreUnchanged() throws {
        for old in ["native", "1080", "900", "720", "540", "1280x720", "4:3"] {
            let s = try JSONDecoder().decode(LaunchSettings.self, from: Data(#"{"screen":"\#(old)"}"#.utf8))
            XCTAssertEqual(s.screen, old)
            XCTAssertNil(s.aspect)
            let e = LaunchSettings.resolve(game: nil, global: s)
            XCTAssertEqual(e.aspect, .phone)
            XCTAssertEqual(e.guestScreen, old)
        }
        let none = LaunchSettings.resolve(game: nil, global: LaunchSettings())
        XCTAssertEqual(none.screen, "720")
        XCTAssertEqual(none.guestScreen, "720")
    }

    func testAGameShapeOverridesTheGlobalOne() throws {
        let global = LaunchSettings(screen: "1080", aspect: "4:3")
        XCTAssertEqual(LaunchSettings.resolve(game: nil, global: global).guestScreen, "4:3@1080")
        XCTAssertEqual(LaunchSettings.resolve(game: LaunchSettings(aspect: "16:9"), global: global).guestScreen, "16:9@1080")
        XCTAssertEqual(LaunchSettings.resolve(game: LaunchSettings(aspect: "phone"), global: global).guestScreen, "1080")
        XCTAssertEqual(LaunchSettings.resolve(game: LaunchSettings(screen: "540"), global: global).guestScreen, "4:3@540")
        // An unknown shape is skipped, as if unset.
        XCTAssertEqual(LaunchSettings.resolve(game: LaunchSettings(aspect: "21:9"), global: LaunchSettings()).aspect, .phone)
        // The --settings JSON form.
        let s = try JSONDecoder().decode(LaunchSettings.self, from: Data(#"{"aspect":"16:10","screen":"900"}"#.utf8))
        XCTAssertEqual(s, LaunchSettings(screen: "900", aspect: "16:10"))
        XCTAssertFalse(s.isEmpty)
        XCTAssertFalse(LaunchSettings(aspect: "16:9").isEmpty)
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import XCTest
@testable import PlayportKit

final class ScreenAspectTests: XCTestCase {
    func testEveryShapeOffersValidResolutionsOfItsShape() {
        for a in ScreenAspect.allCases {
            XCTAssertFalse(a.resolutions.isEmpty, a.rawValue)
            XCTAssertTrue(a.resolutions.contains(a.defaultResolution), a.rawValue)
            for spec in a.resolutions {
                XCTAssertTrue(LaunchPlan.validScreen(spec), spec)
                XCTAssertEqual(ScreenAspect.of(spec), a, spec)
            }
        }
        XCTAssertEqual(ScreenAspect.phone.resolutions, ["native", "1080", "900", "720", "540"])
        XCTAssertEqual(ScreenAspect.wide16x9.resolutions, ["1920x1080", "1600x900", "1280x720", "960x540"])
        XCTAssertEqual(ScreenAspect.allCases.map(\.rawValue), ["phone", "16:9", "16:10", "4:3"])
    }

    /// A screen saved before the aspect ratio choice keeps its meaning: Phone at that resolution.
    func testSavedScreensFromBeforeAreThePhoneShapeUnchanged() throws {
        for old in ["native", "1080", "900", "720", "540", "NATIVE"] {
            let s = try JSONDecoder().decode(LaunchSettings.self, from: Data(#"{"screen":"\#(old)"}"#.utf8))
            XCTAssertEqual(s.screen, old)
            XCTAssertEqual(ScreenAspect.of(s.screen), .phone, old)
            XCTAssertEqual(LaunchSettings.resolve(game: nil, global: s).screen, old)
        }
        XCTAssertEqual(ScreenAspect.of(nil), .phone)
        XCTAssertEqual(LaunchSettings.resolve(game: nil, global: LaunchSettings()).screen, "720")
        // The old hidden 4:3 spec is the 4:3 shape at its 1024x768.
        XCTAssertEqual(ScreenAspect.of("4:3"), .standard4x3)
        XCTAssertEqual(ScreenAspect.canonical("4:3"), "1024x768")
        XCTAssertEqual(ScreenAspect.canonical("720"), "720")
        // A custom size of no offered shape.
        XCTAssertNil(ScreenAspect.of("1564x720"))
        XCTAssertEqual(ScreenAspect.of("2560x1600"), .wide16x10)
    }

    func testAGameScreenOverridesTheGlobalOne() {
        let global = LaunchSettings(screen: "1280x720")
        XCTAssertEqual(LaunchSettings.resolve(game: nil, global: global).screen, "1280x720")
        XCTAssertEqual(LaunchSettings.resolve(game: LaunchSettings(), global: global).screen, "1280x720")
        XCTAssertEqual(LaunchSettings.resolve(game: LaunchSettings(screen: "720"), global: global).screen, "720")
        XCTAssertEqual(LaunchSettings.resolve(game: LaunchSettings(screen: "1024x768"), global: LaunchSettings()).screen, "1024x768")
    }

    func testPickingAShapeKeepsOrResetsTheResolution() {
        // The same shape keeps the level's resolution; another gives that shape's default.
        XCTAssertEqual(ScreenAspect.wide16x9.spec(over: "1920x1080", storesDefault: false), "1920x1080")
        XCTAssertEqual(ScreenAspect.wide16x9.spec(over: "1080", storesDefault: false), "1280x720")
        XCTAssertEqual(ScreenAspect.wide16x10.spec(over: nil, storesDefault: false), "1280x800")
        XCTAssertEqual(ScreenAspect.phone.spec(over: "900", storesDefault: false), "900")
        // Phone at the global level is nothing (Playport's 720p) ...
        XCTAssertNil(ScreenAspect.phone.spec(over: "1280x720", storesDefault: false))
        XCTAssertNil(ScreenAspect.phone.spec(over: nil, storesDefault: false))
        // ... but a game stores it, to override a global 16:9.
        XCTAssertEqual(ScreenAspect.phone.spec(over: nil, storesDefault: true), "720")
        XCTAssertEqual(ScreenAspect.wide16x9.spec(over: nil, storesDefault: true), "1280x720")
        XCTAssertEqual(ScreenAspect.standard4x3.spec(over: "4:3", storesDefault: true), "4:3")
    }

    func testAStepperDefaultResolutionKeepsTheShape() {
        XCTAssertNil(ScreenAspect.phone.spec(resolution: nil))
        XCTAssertEqual(ScreenAspect.phone.spec(resolution: "1080"), "1080")
        XCTAssertEqual(ScreenAspect.wide16x9.spec(resolution: nil), "1280x720")
        XCTAssertEqual(ScreenAspect.wide16x9.spec(resolution: "960x540"), "960x540")
    }
}

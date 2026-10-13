// SPDX-License-Identifier: GPL-3.0-or-later
import XCTest
@testable import PlayportKit

final class JitFailureReasonTests: XCTestCase {
    private func reason(_ raw: String?, ran: Bool = true) -> JitFailureReason {
        JitFailureReason(raw: raw, helperDidNotRun: !ran)
    }

    func testHelperThatNeverRanIsMissingWhateverTheText() {
        XCTAssertEqual(reason("the JIT helper extension is not in the app bundle", ran: false), .helperMissing)
        XCTAssertEqual(reason("the JIT helper did not connect within 5 s", ran: false), .helperMissing)
        XCTAssertEqual(reason(nil, ran: false), .helperMissing)
        XCTAssertEqual(JitFailureReason.helperMissing.message,
                       "The JIT helper is missing: reinstall Playport and keep its app extensions.")
    }

    func testLockdownPairingErrorsAreARejectedPairingFile() {
        for raw in ["Lockdown error: InvalidHostID", "PairingDialogResponsePending", "InvalidPairRecord",
                    "failed to read pairing file"] {
            XCTAssertEqual(reason(raw), .pairingRejected, raw)
        }
        XCTAssertEqual(JitFailureReason.pairingRejected.message, "This pairing file is not accepted: make a new one.")
    }

    func testDownloadFailure() {
        let r = reason("Failed to download Developer Disk Image: The Internet connection appears to be offline.")
        XCTAssertEqual(r, .downloadFailed)
        XCTAssertEqual(r.message, "Could not download the disk image: check the connection.")
    }

    func testDeveloperModeAndMountFailures() {
        XCTAssertEqual(reason("Developer Mode is not enabled"), .developerMode)
        XCTAssertEqual(reason("failed to mount personalized developer disk image"), .mountFailed)
        XCTAssertTrue(JitFailureReason.mountFailed.message.contains("download the disk image again"))
        XCTAssertEqual(JitFailureReason.developerMode.message, "Turn on Developer Mode.")
    }

    func testAnythingElseIsGenericWithTheShortRawReason() {
        XCTAssertEqual(reason("failed to connect to lockdownd"), .other("failed to connect to lockdownd"))
        XCTAssertEqual(reason("failed to connect to lockdownd").message,
                       "Game debugging could not be prepared (failed to connect to lockdownd).")
        XCTAssertEqual(reason(nil).message, "Game debugging could not be prepared.")
        XCTAssertEqual(reason("a\nsecond line"), .other("a"))
        guard case .other(let long) = reason(String(repeating: "x", count: 300)) else { return XCTFail() }
        XCTAssertEqual(long.count, 101)
    }

    func testLogNamesAreDistinct() {
        let all: [JitFailureReason] = [.helperMissing, .pairingRejected, .downloadFailed, .developerMode, .mountFailed, .other("")]
        XCTAssertEqual(Set(all.map(\.logName)).count, all.count)
    }
}

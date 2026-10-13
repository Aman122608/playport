// SPDX-License-Identifier: GPL-3.0-or-later
// Why built-in JIT's readiness check failed, in a player's words. The helper
// and StikJIT report free text; this sorts it into the classes a player can
// act on (docs/DEVICE.md, "JIT activation"). The raw reason always goes to the log.

import Foundation

public enum JitFailureReason: Equatable, Sendable {
    /// The helper extension never ran: removed by a re-signer, or not started.
    case helperMissing
    /// Lockdown refused the pairing file.
    case pairingRejected
    case downloadFailed
    case developerMode
    case mountFailed
    case other(String)

    /// `helperDidNotRun`: the app could not start the helper or reach it (no extension,
    /// the start was refused, no connection, it went away). `raw` is the reason text.
    public init(raw: String?, helperDidNotRun: Bool) {
        let text = (raw ?? "").lowercased()
        func has(_ words: String...) -> Bool { words.contains { text.contains($0) } }
        if helperDidNotRun {
            self = .helperMissing
        } else if has("invalidhostid", "pairingdialog", "invalidpairrecord", "invalid pair record", "read pairing file") {
            self = .pairingRejected
        } else if has("download") {
            self = .downloadFailed
        } else if has("developer mode", "developermode") {
            self = .developerMode
        } else if has("mount") {
            self = .mountFailed
        } else {
            self = .other(Self.shortened(raw))
        }
    }

    /// The first line of the raw reason, at most 100 characters.
    static func shortened(_ raw: String?) -> String {
        let line = (raw ?? "").split(whereSeparator: \.isNewline).first.map(String.init) ?? ""
        let trimmed = line.trimmingCharacters(in: .whitespaces)
        return trimmed.count > 100 ? String(trimmed.prefix(100)) + "…" : trimmed
    }

    /// The short class name for the log line.
    public var logName: String {
        switch self {
        case .helperMissing: "helper-missing"
        case .pairingRejected: "pairing-rejected"
        case .downloadFailed: "download-failed"
        case .developerMode: "developer-mode"
        case .mountFailed: "mount-failed"
        case .other: "other"
        }
    }

    public var message: String {
        switch self {
        case .helperMissing: Self.helperMissingMessage
        case .pairingRejected: "This pairing file is not accepted: make a new one."
        case .downloadFailed: "Could not download the disk image: check the connection."
        case .developerMode: "Turn on Developer Mode."
        case .mountFailed: "The disk image did not mount: turn on Developer Mode, or download the disk image again."
        case .other(let short):
            short.isEmpty ? "Game debugging could not be prepared." : "Game debugging could not be prepared (\(short))."
        }
    }

    public static let helperMissingMessage = "The JIT helper is missing: reinstall Playport and keep its app extensions."
}

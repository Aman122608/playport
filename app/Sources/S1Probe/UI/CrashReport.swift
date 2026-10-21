// SPDX-License-Identifier: GPL-3.0-or-later
// "<game> stopped unexpectedly. The game ended with an error": noticed at the next
// launch, explained, and easy to send on.
//
// A game runs inside Playport's process, so a crash kills the app with it and no
// UI can be shown at that moment. Instead AppRestart's marker (running-game.json)
// is written when a launch starts and removed when it ends the normal way; one
// still there at the next launch means the last session did not end by itself.
// CrashGuard's handler put the reason in the log (`*** FATAL SIGNAL <n>`, a
// backtrace, `*** UNCAUGHT ...`), and this reads it back.
//
// What it is NOT: a second record of why a session ended. A launch that ended by
// itself still reports through AppRestart.Record and the library's alert; this is
// only for the process that never got to say anything.

import Foundation
import SwiftUI

@MainActor
final class CrashReport: ObservableObject {
    static let shared = CrashReport()

    struct Report: Identifiable {
        let id = UUID()
        let game: String
        let titleID: String?
        let cause: Cause
        /// When the launch that did not end started.
        let when: Date
        /// The end of the run's log, from just before the line that says how it stopped.
        let excerpt: [String]
        /// The log the crash is in: the current one, or a release build's previous log when the
        /// launch that found the marker rotated the crashed run's out of the way.
        let log: URL

        /// Exactly the sentence the player is owed.
        var headline: String { "\(game) stopped unexpectedly. The game ended with an error" }
    }

    enum Cause {
        case signal(Int32)
        case exception(String)
        case startFailed
        case unclean

        var detail: String {
            switch self {
            case .signal(let s):
                let name = [SIGSEGV: "SIGSEGV, a bad memory access",
                            SIGABRT: "SIGABRT, the game or a library gave up",
                            SIGILL: "SIGILL, an instruction this device cannot run",
                            SIGFPE: "SIGFPE, an arithmetic error",
                            SIGSYS: "SIGSYS, a system call iOS does not allow"][s] ?? "signal \(s)"
                return "It stopped with \(name). The end of its log is below, and Share Report takes "
                    + "the whole file."
            case .exception(let name):
                return "It stopped on an uncaught \(name). The end of its log is below, and Share "
                    + "Report takes the whole file."
            case .startFailed:
                return "The runtime could not start it. The end of its log is below."
            case .unclean:
                return "Nothing was recorded, which usually means iOS ended Playport for using too much "
                    + "memory, or it was closed from the app switcher."
            }
        }
    }

    /// The report to show once; nil when the last session ended by itself.
    @Published var pending: Report?

    /// The game running now, so the marker can be rewritten when the app comes back in front.
    private var running: AppRestart.GameMarker?

    private static func log(_ line: String) { WineHostRuntime.appendLog("crash: " + line) }

    // MARK: the marker

    /// A launch starts (TitleLaunch.begin): until it ends, this process's death is the game's.
    func gameStarted(title: String, titleID: String?) {
        let marker = AppRestart.GameMarker(title: title, titleID: titleID,
                                           startedMs: Int64((Date().timeIntervalSince1970 * 1000).rounded()),
                                           pid: getpid())
        running = marker
        AppRestart.writeGameMarker(marker)
    }

    /// The launch ended by itself (TitleLaunch.end): nothing to report.
    func gameEnded() {
        running = nil
        AppRestart.gameEnded()
    }

    /// RootView's scene phase. A background app that iOS ends for memory is not the game's doing,
    /// so the marker goes while it is away and comes back with it (if the game is still running).
    /// The cost, taken deliberately: a fault that happens while Playport is in the background is
    /// not reported either -- iOS kills a background app far more often than a game faults there.
    func scenePhase(_ phase: ScenePhase) {
        switch phase {
        case .background:
            if running != nil { AppRestart.gameEnded() }
        case .active:
            if let running, TitleLaunch.shared.running { AppRestart.writeGameMarker(running) }
        default:
            break
        }
    }

    // MARK: the next launch

    /// The app's init: was a game running when Playport last ended? Reads the marker, the log it
    /// left, and sets `pending` for RootView's sheet.
    func noteLaunch() {
        let url = AppRestart.gameMarkerURL
        guard let data = try? Data(contentsOf: url) else { return }
        AppRestart.gameEnded()
        guard let marker = try? JSONDecoder().decode(AppRestart.GameMarker.self, from: data) else {
            Self.log("running-game.json was unreadable; dropping it")
            return
        }
        // This process wrote it and is still alive: not a crash (a marker left by a launch that
        // ended in this process, before TitleLaunch.end could remove it).
        guard marker.pid != getpid() else { return }
        guard let (log, lines) = Self.previousRun() else {
            Self.log("\(marker.title) did not end by itself, but there is no log to read")
            return
        }
        let cause = Self.cause(in: lines)
        // The log's end is where the reason is; the backtrace CrashGuard wrote after it is frames,
        // not context, so the excerpt runs up to the crash line rather than the file's last line.
        let crash = lines.lastIndex { $0.contains("*** FATAL SIGNAL") || $0.contains("*** UNCAUGHT") }
        let excerpt = crash.map { Array(lines[max(0, $0 - 8)...].prefix(16)) } ?? Array(lines.suffix(14))
        pending = Report(game: marker.title, titleID: marker.titleID, cause: cause,
                         when: Date(timeIntervalSince1970: Double(marker.startedMs) / 1000),
                         excerpt: excerpt, log: log)
        Self.log("\(marker.title) did not end by itself last run (\(cause)): \(log.lastPathComponent)")
    }

    /// The log of the run that ended, and its lines. The current log is the crashed run's, unless
    /// the launch that found the marker rotated it away (over half the limit, AppLog.start), in
    /// which case the crash line is in `playport.previous.log`: take whichever holds it.
    private static func previousRun() -> (URL, [String])? {
        var candidates = [AppLog.url]
        if let previous = AppLog.previousURL, FileManager.default.fileExists(atPath: previous.path) {
            candidates.append(previous)
        }
        var first: (URL, [String])?
        for url in candidates {
            guard let data = try? Data(contentsOf: url), !data.isEmpty else { continue }
            // The tail is what matters and a log is up to 4 MiB: read the last 256 KiB.
            let tail = data.count > 262_144 ? data.suffix(262_144) : data
            let lines = String(decoding: tail, as: UTF8.self)
                .split(separator: "\n", omittingEmptySubsequences: true)
                .map { String($0).trimmingCharacters(in: .whitespaces) }
            if first == nil { first = (url, lines) }
            if lines.contains(where: { $0.contains("*** FATAL SIGNAL") || $0.contains("*** UNCAUGHT") }) {
                return (url, lines)
            }
        }
        return first
    }

    private static func cause(in lines: [String]) -> Cause {
        if let fatal = lines.last(where: { $0.contains("*** FATAL SIGNAL") }) {
            // Anchored on the bookkeeping token, not on "the first number in the line": a reason
            // inside a message ("index 5 beyond bounds") is not a signal number.
            let fields = fatal.split(separator: " ").map(String.init)
            if let at = fields.firstIndex(of: "SIGNAL"), fields.count > at + 1, let number = Int32(fields[at + 1]) {
                return .signal(number)
            }
        }
        if let exception = lines.last(where: { $0.contains("*** UNCAUGHT") }),
           let name = Self.exceptionName(in: exception) {
            return .exception(name)
        }
        // The launch reported its end before the process went: a start that failed, not a crash.
        // LaunchCoordinator writes `exit=0x%08x after_s=...` (an exit of 0 is a clean end).
        if let done = lines.last(where: { $0.hasPrefix("title: done") }), !done.contains("exit=0x00000000") {
            return .startFailed
        }
        return .unclean
    }

    /// `*** UNCAUGHT NSRangeException: …` → `NSRangeException`.
    private static func exceptionName(in line: String) -> String? {
        guard let at = line.range(of: "*** UNCAUGHT ") else { return nil }
        let rest = line[at.upperBound...]
        let name = String(rest.prefix { $0 != ":" }).trimmingCharacters(in: .whitespaces)
        return name.isEmpty ? nil : name
    }
}

/// The sheet over the library at the next launch: the sentence, what is known, the end of the
/// log, and the two ways to send it on.
struct CrashReportSheet: View {
    let report: CrashReport.Report

    @Environment(\.dismiss) private var dismiss
    @State private var sending = false

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text(report.headline)
                .font(PP.display(22))
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("crash-headline")
            Text(report.cause.detail)
                .font(.system(size: 13)).foregroundStyle(PP.muted).lineSpacing(2)
                .fixedSize(horizontal: false, vertical: true)

            ScrollView {
                Text(report.excerpt.isEmpty ? "(the log has no lines left for this run)"
                                            : report.excerpt.joined(separator: "\n"))
                    .font(.system(size: 11, design: .monospaced))
                    .foregroundStyle(PP.soft)
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(10)
            }
            .frame(maxHeight: 200)
            .background(PP.surface, in: RoundedRectangle(cornerRadius: 10))

            HStack(spacing: 10) {
                Button {
                    ProblemReport.share(name: report.game)
                } label: {
                    Label("Share Report", systemImage: "square.and.arrow.up")
                }
                .buttonStyle(.bordered)

                Button {
                    sending = true
                } label: {
                    Label("Send to the computer", systemImage: "paperplane")
                }
                .buttonStyle(.borderedProminent)

                Spacer()

                Button("Done") { dismiss() }.buttonStyle(.bordered)
            }
            .font(.system(size: 14))
        }
        .padding(22)
        .frame(maxWidth: 620, maxHeight: .infinity, alignment: .topLeading)
        .foregroundStyle(PP.text)
        .background(PP.background)
        .sheet(isPresented: $sending) {
            SendLogSheet(file: report.log, heading: "Send this crash report")
        }
        .accessibilityIdentifier("crash-report")
    }
}

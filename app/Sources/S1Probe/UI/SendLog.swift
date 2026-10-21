// SPDX-License-Identifier: GPL-3.0-or-later
// "Send log": the app's log, and an optional message, to a listener on this network.
// Settings › About and the in-game menu both open it (UI/SettingsView.swift,
// UI/InGameMenuView.swift), so a player can report a problem without pulling the
// file over USB (tools/afc.py).
//
// Nothing is compiled in: an address in a build is one person's network, so the
// address is *found* -- this device's own /24 is swept on the receiver's port and
// the first machine that answers with the marker is taken -- remembered once
// found, and typed by hand only when the sweep finds nothing. The marker is what
// makes that safe: a different server that merely holds the port does not say it.
//
// Deliberately shaped to be privacy-respecting:
//   * never automatic -- there is no send without a tap on Send;
//   * only to the one address shown on the button;
//   * nothing else is transmitted: no identifiers, no telemetry, no third party.
//
// The other end is tools/playport_log_receiver.py, which answers the probe on
// GET /log and takes raw bytes on POST /log with X-Playport-Build and
// X-Playport-Note (base64 UTF-8) and puts the message at the top of the log it saves.

import Combine
import ContentKit
import Foundation
import SwiftUI

/// Finds `tools/playport_log_receiver.py` on this network.
enum LogReceiver {
    /// The receiver's default port. Another one can still be typed by hand.
    static let port = 8765
    /// What the receiver says on `GET /log`; a server that does not say it is not used.
    static let marker = "playport-log-receiver"

    /// One short request, from anywhere: static and nonisolated, so a task group can run
    /// 254 of these at once without anything crossing an isolation boundary.
    nonisolated static func probe(_ address: String) async -> Bool {
        guard let url = URL(string: "http://\(host(address))/log") else { return false }
        var request = URLRequest(url: url)
        request.timeoutInterval = 1.5
        request.cachePolicy = .reloadIgnoringLocalCacheData
        guard let (data, response) = try? await probeSession.data(for: request),
              let http = response as? HTTPURLResponse, http.statusCode == 200 else { return false }
        return String(decoding: data, as: UTF8.self).contains(marker)
    }

    /// Every address on this device's /24 at once. The first receiver to answer wins and the
    /// rest are cancelled, so the search costs about as long as the slowest single miss.
    nonisolated static func sweep() async -> String? {
        guard let base = subnetPrefix() else { return nil }
        return await withTaskGroup(of: String?.self) { group in
            for n in 1...254 {
                let candidate = "\(base).\(n):\(port)"
                group.addTask { await probe(candidate) ? candidate : nil }
            }
            for await answer in group {
                if let answer {
                    group.cancelAll()
                    return answer
                }
            }
            return nil
        }
    }

    /// Accept what the player might have typed -- a URL, a path, or `host:port` -- as `host:port`.
    nonisolated static func host(_ address: String) -> String {
        var s = address.trimmingCharacters(in: .whitespacesAndNewlines)
        for prefix in ["http://", "https://"] where s.lowercased().hasPrefix(prefix) {
            s = String(s.dropFirst(prefix.count))
        }
        if let slash = s.firstIndex(of: "/") { s = String(s[..<slash]) }
        if s.isEmpty { return s }
        if !s.contains(":") { s += ":\(port)" }
        return s
    }

    /// The first three octets of this device's Wi-Fi address, so the sweep is the /24 it is on.
    /// `en0` is Wi-Fi; 169.254.x is what iOS hands out with no DHCP server, and sweeping that
    /// finds nothing.
    nonisolated static func subnetPrefix() -> String? {
        var addresses: UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&addresses) == 0, let first = addresses else { return nil }
        defer { freeifaddrs(addresses) }
        var cursor: UnsafeMutablePointer<ifaddrs>? = first
        while let interface = cursor?.pointee {
            if let sa = interface.ifa_addr, sa.pointee.sa_family == UInt8(AF_INET) {
                let name = String(cString: interface.ifa_name)
                var text = [CChar](repeating: 0, count: Int(NI_MAXHOST))
                if getnameinfo(sa, socklen_t(sa.pointee.sa_len), &text, socklen_t(text.count),
                               nil, 0, NI_NUMERICHOST) == 0 {
                    let ip = String(cString: text)
                    if name.hasPrefix("en"), !ip.hasPrefix("127."), !ip.hasPrefix("169.254.") {
                        let parts = ip.split(separator: ".")
                        if parts.count == 4 { return parts.prefix(3).joined(separator: ".") }
                    }
                }
            }
            cursor = interface.ifa_next
        }
        return nil
    }
}

/// One session for every probe: a connect to an address with nothing on it fails in
/// milliseconds, so the timeout only has to cover a slow one, and 254 in flight still
/// finish in about a second. `waitsForConnectivity` must be false, or a dead address
/// hangs instead of failing fast.
private nonisolated(unsafe) let probeSession: URLSession = {
    let config = URLSessionConfiguration.ephemeral
    config.timeoutIntervalForRequest = 1.5
    config.timeoutIntervalForResource = 4
    config.waitsForConnectivity = false
    config.allowsCellularAccess = false
    config.requestCachePolicy = .reloadIgnoringLocalCacheData
    config.urlCache = nil
    return URLSession(configuration: config)
}()

/// The send: where it goes, what message rides with it, and what to say afterwards.
/// Shared by the Settings and in-game sheets, so the address and the message typed during a
/// game are the ones the player finds afterwards.
@MainActor
final class LogSender: ObservableObject {
    static let shared = LogSender()

    static let addressKey = "logs.address"
    static let noteKey = "logs.note"

    /// The address as the player sees it: `192.168.1.10:8765`, or whatever they typed.
    @Published var address: String {
        didSet { UserDefaults.standard.set(address, forKey: Self.addressKey) }
    }
    /// The message that goes with the log. Persisted, because the moment worth describing is
    /// usually while the game is still on screen.
    @Published var note: String {
        didSet { UserDefaults.standard.set(note, forKey: Self.noteKey) }
    }

    @Published private(set) var status: String?
    @Published private(set) var failed = false
    @Published private(set) var busy = false
    @Published private(set) var finding = false

    private init() {
        address = UserDefaults.standard.string(forKey: Self.addressKey) ?? ""
        note = UserDefaults.standard.string(forKey: Self.noteKey) ?? ""
    }

    /// The address as it will be used, or nil when there is none yet.
    var target: URL? {
        let host = LogReceiver.host(address)
        guard !host.isEmpty else { return nil }
        return URL(string: "http://\(host)/log")
    }

    /// `host:port`, for the status lines.
    var displayHost: String { LogReceiver.host(address) }

    /// The app's version-build, as the receiver puts it in the file's name.
    static var buildStamp: String {
        let info = Bundle.main.infoDictionary ?? [:]
        let version = (info["CFBundleShortVersionString"] as? String) ?? "playport"
        let build = (info["CFBundleVersion"] as? String) ?? "0"
        return "\(version)-\(build)"
    }

    /// Look for the receiver on this network. The remembered address is tried first: if the
    /// computer is where it was, that answers in milliseconds and nothing is swept.
    func find() async {
        guard !finding else { return }
        finding = true
        defer { finding = false }
        if !address.isEmpty, await LogReceiver.probe(address) {
            status = "\(displayHost) is answering."
            failed = false
            return
        }
        if let found = await LogReceiver.sweep() {
            address = found
            status = "Found the receiver at \(found)."
            failed = false
        } else {
            status = "No log receiver answered on this network. Run tools/playport_log_receiver.py on "
                + "the computer, or type its address."
            failed = true
        }
    }

    /// Send `file` now. One tap, one attempt: a second attempt is a second tap.
    func send(_ file: URL = AppLog.url) async {
        guard !busy else { return }
        busy = true
        defer { busy = false }
        guard let url = target else {
            status = "No address yet. Find your computer, or type its address."
            failed = true
            return
        }
        // The log is private (whatever the game wrote), so the far end has to prove it is the
        // receiver before any of it is sent -- an address typed by hand is checked like a found one.
        let answering = await LogReceiver.probe(address)
        guard answering else {
            status = "\(displayHost) does not answer as a log receiver. Find your computer, or check "
                + "the address. Share the report instead."
            failed = true
            return
        }
        guard let data = try? Data(contentsOf: file), !data.isEmpty else {
            status = "The log could not be read."
            failed = true
            return
        }
        guard data.count <= 40 << 20 else {
            status = "That log is \(data.count >> 20) MB; the receiver takes 40 MB at most."
            failed = true
            return
        }
        let message = note.trimmingCharacters(in: .whitespacesAndNewlines)
        var headers: [String: Secret<String>] = [
            "Content-Type": Secret("text/plain; charset=utf-8"),
            "X-Playport-Build": Secret(Self.buildStamp),
        ]
        // The message rides in a header as base64 of its UTF-8: HTTP headers carry no charset
        // of their own, and a message can hold any character the player can type. The receiver
        // puts it at the top of the saved log.
        if !message.isEmpty {
            headers["X-Playport-Note"] = Secret(Data(message.utf8).base64EncodedString())
        }

        var client = HTTPClient(log: Logger { line in WineHostRuntime.appendLog("send: " + line) })
        client.maxAttempts = 1               // one tap, one attempt
        client.timeout = 60

        status = nil
        failed = false
        do {
            _ = try await client.send(url, method: "POST", headers: headers, body: Secret([UInt8](data)),
                                      maxBytes: 64 << 10, label: "send log")
            failed = false
            status = "Sent \(data.count) bytes to \(displayHost)" + (message.isEmpty ? "." : " with your message.")
            if !message.isEmpty { note = "" }     // a message is consumed by the send that carried it
        } catch {
            failed = true
            status = "Not sent: \(Self.why(error, host: displayHost))"
        }
        WineHostRuntime.appendLog("send: \(failed ? "failed" : "sent \(data.count) bytes") to \(displayHost)")
    }

    /// The one line a player can act on: the HTTP code, or the transport error.
    private static func why(_ error: Error, host: String) -> String {
        let text = (error as? ClientError)?.description ?? "\(error)"
        if let range = text.range(of: "HTTP ") {
            let digits = text[range.upperBound...].prefix { $0.isNumber }
            if !digits.isEmpty { return "the receiver answered \(digits)." }
        }
        if text.contains("transport") || error is URLError {
            return "could not reach \(host) (\(text)). Share the report instead."
        }
        return "\(text). Share the report instead."
    }
}

/// The sheet both Send log rows open: the address, the message, one button.
struct SendLogSheet: View {
    /// What to send: the app's log, unless the crash sheet hands the crashed session's.
    var file: URL = AppLog.url
    /// What the sheet is about, above the address ("Send log", "Send this crash report").
    var heading = "Send log"

    @ObservedObject private var sender = LogSender.shared
    @Environment(\.dismiss) private var dismiss
    @State private var draft = ""
    @State private var editing = false

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack {
                Text(heading).font(PP.display(22))
                Spacer()
                Button("Done") { dismiss() }
            }
            Text("Playport sends this log to a computer on the same network, with your message at the "
                 + "top of it. Nothing else leaves the phone, and only when you tap Send.")
                .font(.system(size: 13)).foregroundStyle(PP.muted).lineSpacing(2)

            if sender.target == nil || editing {
                TextField("192.168.1.10:8765", text: $draft)
                    .textFieldStyle(.roundedBorder)
                    .keyboardType(.URL)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                HStack(spacing: 10) {
                    Button("Save address") {
                        sender.address = draft.trimmingCharacters(in: .whitespacesAndNewlines)
                        editing = false
                    }
                    .buttonStyle(.bordered)
                    Button {
                        Task {
                            await sender.find()
                            if sender.target != nil { editing = false }    // found: the Send button
                        }
                    } label: {
                        Label(sender.finding ? "Looking…" : "Find my computer", systemImage: "wifi")
                    }
                    .buttonStyle(.bordered)
                    .disabled(sender.finding)
                    if sender.target != nil {
                        Button("Cancel") { editing = false }.buttonStyle(.bordered)
                    }
                }
            } else {
                HStack(spacing: 10) {
                    Button {
                        Task { await sender.send(file) }
                    } label: {
                        Label(sender.busy ? "Sending…" : "Send to \(sender.displayHost)", systemImage: "paperplane")
                            .frame(maxWidth: .infinity).padding(.vertical, 6)
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(sender.busy)
                    Button("Change") { draft = sender.address; editing = true }.buttonStyle(.bordered)
                    Button {
                        Task { await sender.find() }
                    } label: { Image(systemName: "wifi") }
                        .buttonStyle(.bordered)
                        .disabled(sender.finding)
                }
            }

            VStack(alignment: .leading, spacing: 4) {
                Text("Message with this log (optional)").font(.caption).foregroundStyle(PP.muted)
                TextField("What you saw, or what to look at", text: $sender.note, axis: .vertical)
                    .lineLimit(2...5)
                    .textFieldStyle(.roundedBorder)
                    .font(.system(size: 14))
            }

            if let status = sender.status {
                Text(status).font(.footnote).foregroundStyle(sender.failed ? Color(hex: 0xFF9A8A) : PP.muted)
                    .fixedSize(horizontal: false, vertical: true)
            }

            Spacer(minLength: 0)
        }
        .padding(22)
        .frame(maxWidth: 560, maxHeight: .infinity, alignment: .topLeading)
        .foregroundStyle(PP.text)
        .background(PP.background)
        .onAppear { if draft.isEmpty { draft = sender.address } }
        // Nobody should have to know an address, so look for the receiver the first time this is
        // opened. Once found it is remembered, and this costs one request.
        .task { if sender.address.isEmpty { await sender.find() } }
        .accessibilityIdentifier("send-log")
    }
}

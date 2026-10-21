// SPDX-License-Identifier: GPL-3.0-or-later
// Capture, not presentation: a game runs inside Playport's process, so when it
// dies the app dies with it and no UI can be shown at that moment. What this
// leaves behind is a line in the log -- `*** FATAL SIGNAL <n>` and a backtrace --
// and the next launch reads it (UI/CrashReport.swift).
//
// - SIGTRAP and SIGBUS are left alone: the JIT guard owns the brk trap, and
//   treating that as fatal would break JIT activation. SIGPIPE stays ignored.
// - The handler does only what is async-signal-safe: write(2) to a descriptor
//   opened and fsynced at install time, then backtrace_symbols_fd. Everything it
//   touches is C memory allocated at install time -- no Swift Array or String,
//   whose access in a -Onone build goes through the exclusivity runtime and can
//   take a lock. abort() can be raised while that lock is held, and a handler
//   that waits then deadlocks instead of dying, losing the line it was writing.
// - The fatal line is flushed BEFORE the backtrace: backtrace_symbols_fd resolves
//   each frame through dyld, which takes dyld's own lock, and a fault raised
//   inside dlopen (runtime start-up is full of them) would otherwise hang the
//   process before the one line worth keeping reached the disk.
// - A handler cannot fix anything: it writes, flushes, and dies the way the
//   signal wanted (SIG_DFL + raise), so the crash report iOS collects stays real.

import Darwin
import Foundation

enum CrashGuard {
    /// The signals that mean the process is gone. Deliberately not SIGTRAP or SIGBUS.
    private static let fatal: [Int32] = [SIGSEGV, SIGABRT, SIGILL, SIGFPE, SIGSYS]
    private static let frameCount = 64
    /// Message slots, indexed by the signal number, so the handler needs no lookup.
    private static let messageSlots = 32

    nonisolated(unsafe) private static var logFD: Int32 = -1
    nonisolated(unsafe) private static var frames: UnsafeMutablePointer<UnsafeMutableRawPointer?>?
    /// `\n--- backtrace ---\n`, and the `*** FATAL SIGNAL <n>` line per signal.
    nonisolated(unsafe) private static var header: UnsafeMutablePointer<CChar>?
    nonisolated(unsafe) private static var messages: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?

    /// Once, in the app's init, before anything else runs (PlayportApp.init).
    static func install() {
        guard logFD < 0 else { return }
        logFD = open(AppLog.path, O_WRONLY | O_APPEND | O_CREAT, 0o644)
        frames = UnsafeMutablePointer<UnsafeMutableRawPointer?>.allocate(capacity: frameCount)
        frames?.initialize(repeating: nil, count: frameCount)
        header = strdup("\n--- backtrace ---\n")
        let table = UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>.allocate(capacity: messageSlots)
        table.initialize(repeating: nil, count: messageSlots)
        for sig in fatal where (0..<messageSlots).contains(Int(sig)) {
            table[Int(sig)] = strdup("\n*** FATAL SIGNAL \(sig) -- process is dying ***\n")
        }
        messages = table
        for sig in fatal { signal(sig, handle) }
        NSSetUncaughtExceptionHandler { exception in
            // Straight to the descriptor, not AppLog.append: that takes an NSLock, and an exception
            // raised on the thread holding it would leave the handler waiting instead of letting
            // the process go.
            let text = "*** UNCAUGHT \(exception.name.rawValue): \(exception.reason ?? "")\n"
                + exception.callStackSymbols.prefix(20).map { "    " + $0 + "\n" }.joined()
            if logFD >= 0 {
                _ = text.withCString { write(logFD, $0, strlen($0)) }
                fsync(logFD)
            }
        }
        AppLog.append("crash: handlers installed (SEGV, ABRT, ILL, FPE, SYS; TRAP and BUS are the JIT guard's)")
        if logFD < 0 { AppLog.append("crash: no descriptor for the log, a fatal signal will only be a crash report iOS keeps") }
    }

    private static let handle: @convention(c) (Int32) -> Void = { received in
        if logFD >= 0 {
            if let table = messages, received >= 0, received < Int32(messageSlots),
               let message = table[Int(received)] {
                _ = write(logFD, message, strlen(message))
            }
            if let header { _ = write(logFD, header, strlen(header)) }
            fsync(logFD)          // on disk before the backtrace, which can wait on dyld's lock
            if let frames {
                let n = backtrace(frames, Int32(frameCount))
                backtrace_symbols_fd(frames, n, logFD)
            }
        }
        signal(received, SIG_DFL)
        raise(received)
    }
}

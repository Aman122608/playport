// SPDX-License-Identifier: GPL-3.0-or-later
// Steam's VC++ redistributable `.cmd` in its standard form, whose installer Playport runs directly
// (decision 0096): what Steam's `cmd` and `start /w` would run, with the same arguments, working
// directory and exit code, without the three x86-64 processes (cmd, conhost, start: about 84 MiB of
// JIT pool head) and the console window they bring. Any other script runs through `cmd` as before.
//
// The form, as the Steamworks Common Redistributables depots ship it (VC++ 2015-2022):
//
//     @ECHO OFF
//     start /w "" "%~dp0\VC_redist.x86.exe" /q /norestart
//     IF %ERRORLEVEL% == 3010 EXIT /B 0
//     EXIT /B %ERRORLEVEL%
//
// Under `cmd` the script's exit code is the installer's, with 3010 turned into 0; both are
// successes (Prerequisite.successCodes), so the installer's own code keeps decision 0083's outcome.

import Foundation

public struct SteamVCRedistCmd: Equatable, Sendable {
    /// The installer's file name, in the script's own folder (`%~dp0`), as the script spells it.
    public var installer: String
    public var arguments: [String]

    public init(installer: String, arguments: [String]) {
        self.installer = installer
        self.arguments = arguments
    }

    /// The script's installer and arguments when its only effective command is
    /// `start /w "" "%~dp0\<VC++ installer>.exe" <arguments>`; nil for anything else.
    ///
    /// - Before it: blank lines, `@echo off`, `rem` and `::` comments.
    /// - After it: blank lines, comments, `if %errorlevel% == 3010 exit /b 0` and
    ///   `exit /b [%errorlevel%]`, which keep the installer's outcome.
    /// - The installer: a file in the script's folder (no subfolder, so the working directory is
    ///   the one `cmd` gives `start`), named `vc_redist…` or `vcredist…` and ending `.exe`.
    /// - The arguments: plain words; no quote, variable, redirection or command separator.
    public static func match(_ text: String) -> SteamVCRedistCmd? {
        var found: SteamVCRedistCmd?
        for raw in text.split(omittingEmptySubsequences: false, whereSeparator: { $0 == "\n" || $0 == "\r\n" }) {
            let line = raw.trimmingCharacters(in: .whitespacesAndNewlines)
            let lower = line.lowercased()
            if line.isEmpty || isComment(lower) { continue }
            if found == nil {
                if lower == "@echo off" || lower == "echo off" { continue }
                guard let start = startLine(line) else { return nil }
                found = start
            } else if !matches(lower, Self.tail) {
                return nil
            }
        }
        return found
    }

    private static func isComment(_ lower: String) -> Bool {
        lower.hasPrefix("::") || lower == "rem" || lower == "@rem" || lower.hasPrefix("rem ") || lower.hasPrefix("@rem ")
            || lower.hasPrefix("rem\t") || lower.hasPrefix("@rem\t")
    }

    private static let start = try! NSRegularExpression(
        pattern: #"^@?start[ \t]+/w(?:ait)?[ \t]+""[ \t]+"%~dp0\\?((?:vc_redist|vcredist)[^"\\/%!:*?<>|]*\.exe)"((?:[ \t]+[^ \t"%!&|<>^()]+)*)[ \t]*$"#,
        options: [.caseInsensitive])

    private static let tail = try! NSRegularExpression(
        pattern: #"^@?(?:if[ \t]+%errorlevel%[ \t]*==[ \t]*3010[ \t]+exit[ \t]+/b[ \t]+0|exit[ \t]+/b(?:[ \t]+%errorlevel%)?)$"#,
        options: [.caseInsensitive])

    private static func matches(_ s: String, _ re: NSRegularExpression) -> Bool {
        re.firstMatch(in: s, range: NSRange(s.startIndex..., in: s)) != nil
    }

    private static func startLine(_ line: String) -> SteamVCRedistCmd? {
        guard let m = start.firstMatch(in: line, range: NSRange(line.startIndex..., in: line)),
              let exe = Range(m.range(at: 1), in: line), let args = Range(m.range(at: 2), in: line) else { return nil }
        return SteamVCRedistCmd(installer: String(line[exe]),
                                arguments: line[args].split(whereSeparator: { $0 == " " || $0 == "\t" }).map(String.init))
    }
}

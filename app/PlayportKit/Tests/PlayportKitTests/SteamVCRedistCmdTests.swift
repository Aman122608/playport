// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import XCTest
@testable import PlayportKit

/// Steam's VC++ `.cmd` in its standard form (decision 0096). `vc2019x86` and `vc2019x64` are the
/// scripts of the Steamworks Common Redistributables VC++ 2019 depot, byte for byte (CRLF endings).
final class SteamVCRedistCmdTests: XCTestCase {
    static let vc2019x86 = "@ECHO OFF\r\nstart /w \"\" \"%~dp0\\VC_redist.x86.exe\" /q /norestart\r\n"
        + "IF %ERRORLEVEL% == 3010 EXIT /B 0\r\nEXIT /B %ERRORLEVEL%\r\n"
    static let vc2019x64 = "@ECHO OFF\r\nstart /w \"\" \"%~dp0\\VC_redist.x64.exe\" /q /norestart\r\n"
        + "IF %ERRORLEVEL% == 3010 EXIT /B 0\r\nEXIT /B %ERRORLEVEL%\r\n"

    func testSteamsVCRedistScriptsMatchWithTheirInstallerAndArguments() {
        XCTAssertEqual(SteamVCRedistCmd.match(Self.vc2019x86),
                       SteamVCRedistCmd(installer: "VC_redist.x86.exe", arguments: ["/q", "/norestart"]))
        XCTAssertEqual(SteamVCRedistCmd.match(Self.vc2019x64),
                       SteamVCRedistCmd(installer: "VC_redist.x64.exe", arguments: ["/q", "/norestart"]))
    }

    func testCommentsBlankLinesCaseAndLineEndingsDoNotMatter() {
        let text = ":: VC++ 2015-2022\nrem quiet\n\n@echo off\n  START /WAIT \"\" \"%~DP0vcredist_x64.exe\" /install /quiet  \n"
            + "@rem done\nexit /b\n"
        XCTAssertEqual(SteamVCRedistCmd.match(text),
                       SteamVCRedistCmd(installer: "vcredist_x64.exe", arguments: ["/install", "/quiet"]))
        XCTAssertEqual(SteamVCRedistCmd.match("start /w \"\" \"%~dp0\\VC_redist.x86.exe\""),
                       SteamVCRedistCmd(installer: "VC_redist.x86.exe", arguments: []))
    }

    func testAnythingElseRunsThroughCmd() {
        let other = [
            "",                                                                        // no command
            "@ECHO OFF\r\n",                                                           // no command
            "start \"\" \"%~dp0\\VC_redist.x86.exe\" /q",                              // no /w: cmd does not wait
            "start /w \"%~dp0\\VC_redist.x86.exe\" /q",                                // no empty title
            "start /w \"\" \"%~dp0\\sub\\VC_redist.x86.exe\" /q",                      // another folder
            "start /w \"\" \"C:\\VC_redist.x86.exe\" /q",                              // not beside the script
            "start /w \"\" \"%~dp0\\..\\VC_redist.x86.exe\" /q",                       // outside the folder
            "start /w \"\" \"%~dp0\\dotNetFx48.exe\" /q",                              // not a VC++ installer
            "start /w \"\" \"%~dp0\\VC_redist.x86.cmd\" /q",                           // not an executable
            "start /w \"\" \"%~dp0\\VC_redist.x86.exe\" /q %1",                        // a variable
            "start /w \"\" \"%~dp0\\VC_redist.x86.exe\" /log \"%TEMP%\\vc.log\"",      // a quoted argument
            "start /w \"\" \"%~dp0\\VC_redist.x86.exe\" /q > log.txt",                 // a redirection
            "start /w \"\" \"%~dp0\\VC_redist.x86.exe\" /q & reg add HKLM\\X",         // a second command
            "reg add HKLM\\X\r\nstart /w \"\" \"%~dp0\\VC_redist.x86.exe\" /q",        // a command before
            "start /w \"\" \"%~dp0\\VC_redist.x86.exe\" /q\r\nreg add HKLM\\X",        // a command after
            "start /w \"\" \"%~dp0\\VC_redist.x86.exe\" /q\r\nIF %ERRORLEVEL% == 1638 EXIT /B 0", // another code mapped
            "start /w \"\" \"%~dp0\\VC_redist.x86.exe\" /q\r\nexit /b 0",              // the code dropped
            "start /w \"\" \"%~dp0\\VC_redist.x86.exe\" /q\r\nstart /w \"\" \"%~dp0\\VC_redist.x64.exe\" /q", // two
        ]
        for text in other { XCTAssertNil(SteamVCRedistCmd.match(text), text) }
    }

    var games: URL!

    override func setUpWithError() throws {
        games = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
    }

    override func tearDown() {
        try? FileManager.default.removeItem(at: games)
    }

    func write(_ rel: String, _ text: String = "") throws {
        let url = games.appendingPathComponent(rel)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try Data(text.utf8).write(to: url)
    }

    func testAStandardVCRedistEntryRunsItsInstallerDirectlyAndAnyOtherCmdThroughCmd() throws {
        let dir = "Steamworks Shared/_CommonRedist/vcredist/2022/"
        try write(dir + "Microsoft Visual C++ 2022 x86.cmd", Self.vc2019x86)
        try write(dir + "vc_redist.x86.exe")          // matched without case, as the guest sees it
        try write(dir + "Microsoft Visual C++ 2022 x64.cmd", Self.vc2019x64 + "reg add HKLM\\X\r\n")
        try write(dir + "VC_redist.x64.exe")
        var s = SteamPrerequisites()
        s.add(try SteamInstallScript.parse(Data(SteamInstallScriptTests.vc2022.utf8)), appID: 945360,
              installDir: "Steamworks Shared", script: "_CommonRedist/vcredist/2022/installscript.vdf", games: games, hive: "")
        let win = #"Games\Steamworks Shared\_CommonRedist\vcredist\2022\"#
        let hasRun = { (name: String) in
            [PrefixRegistryValue(hive: .system, key: #"Software\Valve\Steam\Apps\CommonRedist\vcredist\2022"#, name: name,
                                 data: .dword(1))]
        }
        XCTAssertEqual(s.prerequisites, [
            Prerequisite(ids: [#"HKEY_LOCAL_MACHINE\Software\Valve\Steam\Apps\CommonRedist\vcredist\2022|x86 14.51.36247.0"#],
                         name: "Steam x86 14.51.36247.0 (_CommonRedist/vcredist/2022/installscript.vdf)",
                         exe: win + "vc_redist.x86.exe", args: ["/q", "/norestart"],
                         successRegistry: hasRun("x86 14.51.36247.0"), standardScript: win + "Microsoft Visual C++ 2022 x86.cmd"),
            Prerequisite(ids: [#"HKEY_LOCAL_MACHINE\Software\Valve\Steam\Apps\CommonRedist\vcredist\2022|x64 14.51.36247.0"#],
                         name: "Steam x64 14.51.36247.0 (_CommonRedist/vcredist/2022/installscript.vdf)",
                         exe: win + "Microsoft Visual C++ 2022 x64.cmd", args: [],
                         successRegistry: hasRun("x64 14.51.36247.0")),
        ], "x86 is the standard form; x64's script does more, so it runs through cmd")
    }

    func testAStandardScriptWhoseInstallerIsMissingRunsThroughCmd() throws {
        let dir = "Shared/_CommonRedist/vcredist/2022/"
        try write(dir + "Microsoft Visual C++ 2022 x86.cmd", Self.vc2019x86)
        var s = SteamPrerequisites()
        s.add(try SteamInstallScript.parse(Data(SteamInstallScriptTests.vc2022.utf8)), appID: 1, installDir: "Shared",
              script: "v.vdf", games: games, hive: "")
        XCTAssertEqual(s.prerequisites.map(\.exe), [#"Games\Shared\_CommonRedist\vcredist\2022\Microsoft Visual C++ 2022 x86.cmd"#])
        XCTAssertNil(s.prerequisites.first?.standardScript)
    }
}

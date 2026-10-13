// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import XCTest
@testable import PlayportKit

/// GOG install scripts (decision 0103): the bodies below are GOG's own `goggame-<id>.script` files, as
/// the owner's installs of Tomb Raider: Anniversary, RIOT - Civil Unrest, Baldur's Gate: Enhanced
/// Edition, BioShock and Spelunky hold them.
final class GOGInstallScriptTests: XCTestCase {
    static let tombRaiderAnniversary = #"""
    {
        "productId": "1202885143",
        "actions": [
            {
                "name": "saveFolders",
                "action": "",
                "install": {
                    "action": "savePath",
                    "arguments": {
                        "savePath": "{userdocs}/Eidos/Tomb Raider - Anniversary",
                        "type": "folder"
                    }
                }
            },
            {
                "name": "registryKey",
                "action": "",
                "install": {
                    "action": "setRegistry",
                    "arguments": {
                        "deleteSubkeys": true,
                        "root": "HKLM",
                        "subkey": "Software\\Crystal Dynamics\\Tomb Raider: Anniversary"
                    }
                }
            },
            {
                "name": "registryKey0",
                "action": "",
                "install": {
                    "action": "setRegistry",
                    "arguments": {
                        "deleteSubkeys": true,
                        "root": "HKCU",
                        "saveGameData": true,
                        "subkey": "Software\\Crystal Dynamics\\Tomb Raider: Anniversary"
                    }
                }
            },
            {
                "name": "registryKey1",
                "action": "",
                "install": {
                    "action": "setRegistry",
                    "arguments": {
                        "root": "HKLM",
                        "subkey": "Software\\Crystal Dynamics\\Tomb Raider: Anniversary",
                        "valueData": "{app}",
                        "valueName": "InstallPath",
                        "valueType": "string"
                    }
                }
            },
            {
                "languages": [
                    "*"
                ],
                "name": "registryKey2",
                "action": "",
                "install": {
                    "action": "setRegistry",
                    "arguments": {
                        "root": "HKLM",
                        "subkey": "Software\\Crystal Dynamics\\Tomb Raider: Anniversary",
                        "valueData": "1",
                        "valueName": "FullInstall",
                        "valueType": "dword"
                    }
                }
            },
            {
                "languages": [
                    "*"
                ],
                "name": "registryKey3",
                "action": "",
                "install": {
                    "action": "setRegistry",
                    "arguments": {
                        "root": "HKLM",
                        "subkey": "Software\\Crystal Dynamics\\Tomb Raider: Anniversary",
                        "valueData": "256",
                        "valueName": "Version",
                        "valueType": "dword"
                    }
                }
            },
            {
                "languages": [
                    "*"
                ],
                "name": "registryKey4",
                "action": "",
                "install": {
                    "action": "setRegistry",
                    "arguments": {
                        "root": "HKCU",
                        "subkey": "Software\\Crystal Dynamics\\Tomb Raider: Anniversary",
                        "valueData": "1",
                        "valueName": "ExtendedDialog",
                        "valueType": "dword"
                    }
                }
            },
            {
                "name": "install_sdb",
                "action": "",
                "install": {
                    "action": "installSDB",
                    "arguments": {
                        "GUID": "{dae86ddb-6063-441d-85c3-5bbc2cdf9486}",
                        "sdbFile": "{app}/goggame.sdb"
                    }
                }
            },
            {
                "name": "regUX",
                "action": "",
                "install": {
                    "action": "Execute",
                    "arguments": {
                        "conditions": [
                            "onlyOnce"
                        ],
                        "executable": "{supportDir}/add/gameux.cmd",
                        "runAsAdmin": true,
                        "workingDir": "{supportDir}/add"
                    }
                }
            }
        ]
    }
    """#

    static let riot = #"""
    {
        "actions": [
            {
                "install": {
                    "action": "savePath",
                    "arguments": {
                        "savePath": "{localappdata}/../LocalLow/DefaultCompany/Riot",
                        "type": "folder"
                    }
                },
                "languages": [
                    "*"
                ],
                "name": "saveFolders"
            },
            {
                "install": {
                    "action": "setRegistry",
                    "arguments": {
                        "root": "HKEY_CURRENT_USER",
                        "saveGameData": true,
                        "subkey": "Software/DefaultCompany/Riot"
                    }
                },
                "languages": [
                    "*"
                ],
                "name": "registryKey1"
            }
        ],
        "productId": "1989052770"
    }
    """#

    static let baldursGate = #"""
    {
        "actions": [
            {
                "install": {
                    "action": "savePath",
                    "arguments": {
                        "savePath": "{userdocs}/Baldur's Gate - Enhanced Edition",
                        "type": "folder"
                    }
                },
                "languages": [
                    "en-US"
                ],
                "name": "saveFolders",
                "osBitness": [
                    "64"
                ]
            }
        ],
        "productId": "1207666353"
    }
    """#

    static let spelunky = #"""
    {
        "actions": [
            {
                "install": {
                    "action": "Execute", 
                    "arguments": {
                        "executable": "galaxy_spelunky_2.1.0.9.exe", 
                        "runAsAdmin": true
                    }
                }, 
                "name": "Install the game"
            }
        ], 
        "productId": "1207659257"
    }
    """#

    static let bioShock = #"""
    {
        "actions": [
            {
                "install": {
                    "action": "savePath",
                    "arguments": {
                        "savePath": "{userdocs}/Bioshock",
                        "type": "folder"
                    }
                },
                "languages": [
                    "*"
                ],
                "name": "saveFolders"
            },
            {
                "install": {
                    "action": "savePath",
                    "arguments": {
                        "savePath": "{userappdata}/Bioshock",
                        "type": "folder"
                    }
                },
                "languages": [
                    "*"
                ],
                "name": "configFolders"
            },
            {
                "install": {
                    "action": "setIni",
                    "arguments": {
                        "filename": "{userappdata}/Bioshock/Bioshock.ini",
                        "keyName": "Language",
                        "keyType": "string",
                        "keyValue": "int",
                        "section": "Engine.Engine",
                        "utf8": false
                    }
                },
                "languages": [
                    "en-US"
                ],
                "name": "langINI"
            },
            {
                "install": {
                    "action": "setIni",
                    "arguments": {
                        "filename": "{app}/Builds/Release/Default.ini",
                        "keyName": "Language",
                        "keyType": "string",
                        "keyValue": "int",
                        "section": "Engine.Engine",
                        "utf8": false
                    }
                },
                "languages": [
                    "en-US"
                ],
                "name": "langINI2"
            },
            {
                "install": {
                    "action": "Execute",
                    "arguments": {
                        "conditions": [
                            "onlyOnce"
                        ],
                        "executable": "{app}/Builds/Release/setres.exe",
                        "runAsAdmin": true,
                        "workingDir": "{app}/Builds/Release"
                    }
                },
                "languages": [
                    "*"
                ],
                "name": "SetRes"
            }
        ],
        "productId": "2022341186"
    }
    """#

    static let folder = #"C:\Games\Tomb Raider Anniversary"#

    func plan(_ body: String, language: String = "en-US") throws -> GOGInstallScript.Plan {
        try GOGInstallScript.parse(Data(body.utf8)).plan(folder: Self.folder, language: language)
    }

    func testTombRaiderAnniversarysRegistryValuesInBothViewsOfHKLM() throws {
        let s = try GOGInstallScript.parse(Data(Self.tombRaiderAnniversary.utf8))
        XCTAssertEqual(s.productID, "1202885143")
        XCTAssertEqual(s.actions.map(\.kind), ["savePath", "setRegistry", "setRegistry", "setRegistry", "setRegistry", "setRegistry",
                                               "setRegistry", "installSDB", "Execute"])
        XCTAssertEqual(s.actions[1].arguments["deleteSubkeys"], "1", "a boolean argument reads as text")
        let p = try plan(Self.tombRaiderAnniversary)
        let key = #"Software\Crystal Dynamics\Tomb Raider: Anniversary"#
        let wow = #"Software\Wow6432Node\Crystal Dynamics\Tomb Raider: Anniversary"#
        XCTAssertEqual(p.registry, [
            PrefixRegistryValue(hive: .system, key: key, name: "InstallPath", data: .string(Self.folder)),
            PrefixRegistryValue(hive: .system, key: wow, name: "InstallPath", data: .string(Self.folder)),
            PrefixRegistryValue(hive: .system, key: key, name: "FullInstall", data: .dword(1)),
            PrefixRegistryValue(hive: .system, key: wow, name: "FullInstall", data: .dword(1)),
            PrefixRegistryValue(hive: .system, key: key, name: "Version", data: .dword(256)),
            PrefixRegistryValue(hive: .system, key: wow, name: "Version", data: .dword(256)),
            PrefixRegistryValue(hive: .user, key: key, name: "ExtendedDialog", data: .dword(1)),
        ])
        XCTAssertEqual(p.notes.count, 5, "savePath, the two value-less keys, installSDB and Execute are left out: \(p.notes)")
        XCTAssertTrue(p.notes.contains { $0.contains("Execute {supportDir}/add/gameux.cmd") })
    }

    func testValuelessKeysSavePathsAndASetupsExecuteWriteNothing() throws {
        let riot = try plan(Self.riot)
        XCTAssertEqual(riot.registry, [], "RIOT's key only marks its saves (saveGameData)")
        XCTAssertEqual(riot.notes.count, 2)
        let bg = try plan(Self.baldursGate)
        XCTAssertEqual(bg.registry, [])
        XCTAssertEqual(bg.notes.count, 1, "its one action, the en-US 64-bit savePath")
        XCTAssertEqual(try plan(Self.baldursGate, language: "fr-FR").notes, [], "an action for another language is not one")
        let sp = try plan(Self.spelunky)
        XCTAssertEqual(sp.registry, [])
        XCTAssertEqual(sp.notes, ["1207659257 Install the game: Execute galaxy_spelunky_2.1.0.9.exe is not run from the script; left out"],
                       "its setup is the build's temp_executable, which decision 0099 runs")
    }

    func testBioShocksIniKeysAndSetupAreLeftOut() throws {
        let p = try plan(Self.bioShock)
        XCTAssertEqual(p.registry, [])
        XCTAssertEqual(p.notes.count, 5, "two savePaths, two setIni (en-US), one Execute: \(p.notes)")
        XCTAssertTrue(p.notes.contains("2022341186 langINI2: setIni {app}/Builds/Release/Default.ini [Engine.Engine] Language=int is not written; left out"))
        XCTAssertEqual(try plan(Self.bioShock, language: "de-DE").notes.count, 3, "its setIni actions are for en-US")
    }

    func testValueForms() throws {
        func one(_ args: String, bitness: String = "") throws -> GOGInstallScript.Plan {
            try plan(#"{"productId":"1","actions":[{"name":"r",ATTR "install":{"action":"setRegistry","arguments":{ARGS}}}]}"#
                .replacingOccurrences(of: "ARGS", with: args).replacingOccurrences(of: "ATTR", with: bitness))
        }
        XCTAssertEqual(try one(#""root":"HKEY_CURRENT_USER","subkey":"Software/A/B","valueName":"P","valueData":"{app}/bin/x.exe""#).registry,
                       [PrefixRegistryValue(hive: .user, key: #"Software\A\B"#, name: "P", data: .string(Self.folder + #"\bin\x.exe"#))],
                       "slashes as backslashes, {app} the game's folder; HKCU is not redirected; string is the default type")
        XCTAssertEqual(try one(#""root":"HKLM","subkey":"Software\\Classes\\x","valueName":"","valueData":"0x10","valueType":"dword""#).registry,
                       [PrefixRegistryValue(hive: .system, key: #"Software\Classes\x"#, name: "", data: .dword(16))],
                       "the default value; Software\\Classes is shared by both views")
        XCTAssertEqual(try one(#""root":"HKLM","subkey":"System\\X","valueName":"v","valueData":"a""#).registry.count, 1,
                       "outside Software, one view")
        XCTAssertEqual(try one(#""root":"HKLM","subkey":"Software\\X","valueName":"v","valueData":"{userdocs}/Saves""#).registry, [],
                       "a folder Playport does not map is left out")
        XCTAssertEqual(try one(#""root":"HKLM","subkey":"Software\\X","valueName":"v","valueData":"ab","valueType":"dword""#).registry, [])
        XCTAssertEqual(try one(#""root":"HKLM","subkey":"Software\\X","valueName":"v","valueData":"ab","valueType":"binary""#).registry, [])
        XCTAssertEqual(try one(#""root":"HKEY_CLASSES_ROOT","subkey":"X","valueName":"v","valueData":"a""#).registry, [])
        XCTAssertEqual(try one(#""root":"HKLM","subkey":"S","valueName":"v","valueData":"a""#, bitness: #""osBitness":["32"],"#).notes, [],
                       "an action for 32-bit Windows only is not one")
    }

    func testNotAScript() {
        XCTAssertThrowsError(try GOGInstallScript.parse(Data("[]".utf8)))
        XCTAssertThrowsError(try GOGInstallScript.parse(Data(#"{"productId":"1"}"#.utf8)))
        XCTAssertEqual(try GOGInstallScript.parse(Data([0xEF, 0xBB, 0xBF] + Array(#"{"productId":"1","actions":[]}"#.utf8))).actions, [],
                       "a byte-order mark is read past")
    }

    func testScriptFilesAreTheRootsGoggameScripts() {
        XCTAssertEqual(GOGInstallScript.scriptFiles(["goggame-1202885143.script", "goggame-1202885143.info", "add/gameux.cmd",
                                                     "sub/goggame-1.script", "goggame-x.script", "GOGGAME-7.SCRIPT"]),
                       ["goggame-1202885143.script", "GOGGAME-7.SCRIPT"])
    }
}

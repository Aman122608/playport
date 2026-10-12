# Evidence: a GOG build's support setup runs from Play; the i386 Inno Setup does not finish yet (PLA-187)

**Date:** 2026-10-12. **Kind:** GOG content-system reads and desktop Wine runs on the workstation,
phone runs (dev app). **Linear:** [PLA-187](https://linear.app/playportdev/issue/PLA-187)
(Neverwinter Nights Diamond, GOG 1207658890). **Decision:** [0099](../decisions/0099-gog-support-setups.md).
The owner was away; the conservative option was taken at each choice. Run directories are under
`$PLAYPORT_BUILD/ui-runs/`, scratch under `$PLAYPORT_BUILD/agent-notes/sweep11/`.

**IPAs:** `Playport-26.5-4b3b3b50.ipa` (dev, sha256
`4b3b3b507cdb992ca9b2b432be13914e265d90a81008098ad959a1281005a5fa`, before the change) and
`Playport-26.5-12062a00.ipa` (dev, sha256
`12062a00f1624405dc4fa8e529eab303ed4b29ef6036e56bcafb4428702c8bbd`, commit 16ffed2, with it).

## Where `binkw32.dll` is

`nwmain.exe` imports `binkw32.dll` (`0xc0000135` at once, run `20261011T173249`). GOG's newest
generation-2 build (`49915841255083197`, "1.69 (A)") has it only as a `support`-flagged depot
file, `app/binkw32.dll`, with `app/nwn.ini`, `app/nwncdkey.ini`, `app/nwnpatch.ini`,
`app/nwnplayer.ini`, `app/nwtoolset.ini`, and `binkw32_ati/binkw32.dll` (a 4,608-byte shim) with
`binkw32_ati/binkw32_ati.dll`. Playport installed them where the depot puts them, so the folder
root had no `binkw32.dll`. Nothing was dropped.

The build manifest's `products[]` names `temp_executable: galaxy_nwn_diamond_2.1.0.21.exe` (the
neutral depot's only file, flagged `support` and `executable`) and `script:
goggame-1207658890.script`, whose one action is `Execute` of that file with `runAsAdmin`. The file
is an Inno Setup 5.5.5 (u) installer, i386. Under desktop Wine (scratch prefix, `xvfb-run`):

| Command line | Outcome |
| --- | --- |
| `/VERYSILENT /SUPPRESSMSGBOXES /DIR=… /LOG=…` | `InitializeSetup returned False; aborting.` |
| Heroic's: `/VERYSILENT /DIR=… /Language=English /LANG=english /ProductId=1207658890 /galaxyclient /buildId=… /versionName=… /nodesktopshorctut /nodesktopshortcut /NOICONS` (+ `/SUPPRESSMSGBOXES`) | installs: `binkw32.dll` and the five `.ini` files into the folder, `HKLM\Software\Wow6432Node\BioWare\NWN\…` and `…\GOG.com\Games\1207658890` keys, an uninstaller, Start-menu links. On this workstation's AMD GPU it chose the ATI shim (`binkw32.dll` 4,608 bytes and `binkw32_ati.dll`) and added two ATI lines to `nwn.ini` |
| Heroic's, run from the game's own folder with spaces (`C:\Games\NWN Diamond`, as Playport places it) | installs, the same |

## The depot language

The build's depots name `en`, `fr`, `de`, `it`, `es`, `pl`. The selection looked for `en-US`,
found none and took the first other one: the install on `4b3b3b50` (run `s11-nwn-install`) was
4.27 GB, the French depot (`goggame-1207658890.info` says `"language": "French"`). With
0099's match on the primary tag, the reinstall on `12062a00` (run `s11-nwn-reinstall`, after an
uninstall through the UI) was 5.09 GB, the English depot. Host test
`testALanguageWithoutARegionIsTheRegionedOnes`.

## Phone runs on `12062a00`

| Run | What | Outcome |
| --- | --- | --- |
| `s11-nwn-play` | Play, default settings (`x87reduced=1`) | the setup started as a prerequisite with the command line above (`gog-support:1207658890@49915841255083197`). Its Inno loader showed an `Error` box, `Path not fpund.`, and waited; no person, so the Play ran to `--wait` 90 s. The loader's failing call was `NtCreateFile` of `\??\D:\Games\NWN Diamond\galaxy_nwn_diamond_2.1.0.210exe`: `C`→`D`, `.`→`0`, and `o`→`p` in the message, each character rounded up to a multiple of 4 |
| `s11-nwn-x87` | the same with the page's x87 precision full (`--settings '{"x87Reduced":false}'`, for the session) | the loader extracted and started `C:\windows\temp\is-….tmp\galaxy_nwn_diamond_2.1.0.21.tmp`; it exited `1` after 1 s, its log (`C:\windows\temp\Setup Log 2026-10-12 #001.txt`): `Runtime Error (at -1:0): Cannot Import EXPANDCONSTANT.` The failure is recorded (once more per app build), the game started and exited `0xc0000135`, as before |

The corrupted path is the signature of a 64-bit integer held as a double: Delphi's `Move` copies
8 bytes with `fild qword` and `fistp qword`, and the UTF-16 text `C:\G` as an integer is about
2^54, so a double keeps it to a multiple of 4. `patches/fex` 0025 keeps a `fild m64` integer
exact only when its `fistp` is in the same block; Delphi's `Move` for longer copies loads the first
8 bytes before its loop and stores them after it. The same `Path not fpund.` box is in the
2026-10-06 installer spike's run (`20261006T141744`, Shogun Showdown's offline Inno Setup 5.6.2).
With 80-bit x87 the loader works, and the setup's PascalScript then fails to bind
`ExpandConstant`, which desktop Wine's run of the same file does not: a second i386 emulation
difference, not traced.

## Outcome

Not fixed on the phone: NWN Diamond still exits `0xc0000135`. The install side matches GOG
Galaxy's and Heroic's (English depot, support files, the setup run with Heroic's command line);
the setup itself does not complete under FEX's i386 route. Next steps, as follow-ups:

- FEX: a `fild m64` integer kept exact across blocks (Delphi `Move`), or the setup run at 80-bit
  x87 (a per-process x87 choice for a prerequisite).
- Under 80-bit x87, why the Inno setup's PascalScript cannot import `ExpandConstant` under FEX
  i386 (the same file binds it under desktop Wine).
- Until then a person sees the loader's `Path not fpund.` box once per app build on NWN Diamond's
  first Play and presses OK; the game then starts and fails as before.

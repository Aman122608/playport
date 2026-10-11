# GDI windows before the first frame: Tomb Raider's launcher

**Linear:** [PLA-152](https://linear.app/playportdev/issue/PLA-152) (this work), which blocks
[PLA-151](https://linear.app/playportdev/issue/PLA-151) (Tomb Raider's launcher);
title [PLA-150](https://linear.app/playportdev/issue/PLA-150/tomb-raider-203160).
Decision [0075](../decisions/0075-guest-windows-before-the-first-frame.md).

Dev IPA `2b2eeb5e8afbecc7e1f8f8478a293a936a454976cc212beefba9bf3fa0a552d9` (`madeira-unix`
0109, `madeira-winios` 0004, `HostIO.swift` `GuestWindows`), installed as an in-place
upgrade. Run directories are under `$PLAYPORT_BUILD/agent-notes/pla152/`. Every Tomb Raider
play: Vulkan, aspect ratio 16:9 (1280x720), no launch arguments.

## The symptom (before)

IPA `d18ceb392ed6cace244c80776d30413e15fc85c4bd18c17634e38f933947ec1f` (main 6ad89e8),
`tr-before`: game started +3.16 s, no first frame through +63 s, the launch screen up
(screenshot at 30 s). The launcher is a 790x445 popup, centred, with one 132x38 child and
no surface on the phone:

```
[win-pos] #1 hwnd=0x10046 after=0x0 flags=0000003c vis={245,137,1035,582} ...
[win-text] hwnd=0x10046 root=0x10046 kind=caption class=c021 style=84880000 text="Tomb Raider"
[win-pos] #6 hwnd=0x10050 after=0x0 flags=00000014 vis={0,0,132,38} surface=0x0
```

The same files under desktop Proton on the sway display (`desktop-runs/launcher`) show it:
the game's art, the logo and an owner-drawn menu (Play, Options, two YouTube links, Web,
Technical Support, Quit).

## With the change

- **`final-tr`** (touch, through the dev driver's new `tap:` action): `first window` +4.10 s,
  `[guest-window] hwnd=0x10046 shown: 790x445 at (245,137)`, `showing desktop (245,137
  790x445) in (83,0 746x420) pt`: the launcher fills the screen's height (screenshot
  `action-02-wait_15.png`, the menu readable). Options opened the 606x377 "Tomb Raider -
  Setup" dialog over it, the view widened to their union (`showing desktop (245,52 790x530)`,
  `action-04-wait_5.png`: Fullscreen, Display, Resolution 1280 x 720, Refresh 120 Hz, Ok,
  Cancel). Ok closed it; Play hid the launcher; the game's own window was shown for a
  moment, then `the game presents: 0 window(s) were shown; none from now on` and `first
  frame` +69.92 s (the taps waited for screenshots; about 2 s after Play). At first
  frame+30 the screenshot is the game's main menu (Continue, New Game, ..., Quit Game;
  Coastal Forest).
- **`final-pad`** (the scripted controller): `first window` +4.03 s; Down, A opened Options
  (`hostio: window keys for down`, `... for a`); B (Escape) closed it; Up, A played:
  `first frame` +41.49 s, main menu on the screenshot.
- Hollow Knight **`final-hk`**: `first frame` +9.92 s (`tomb-raider/main-merge-hk` on 6ad89e8: +10.75 s), no
  window shown, normal main menu at first frame+10. Portal 2 **`final-p2`**: `first frame`
  +5.87 s (`tomb-raider/p2-check`, before this change: +5.98 s), the intro at first frame+10. In both the only line is
  `[guest-window] the game presents: 0 window(s) were shown`.
- `pp test` passed (186 Swift tests including `WindowKeysTests`, tools/tests with the
  `tap:` grammar); `pp build` passed 80 IPA checks; `pp check --variant release` compiles.

Earlier IPAs gave the same results: `f3f978f7…` (the layer hidden instead of removed at the
first frame; `tr-5`, `tr-pad`, Hollow Knight +10.16 s, Portal 2 +8.26 s) and `5a47a493…`
(`trf`, `trf-pad`, `hk-b` +10.06 s, `p2-b` +5.93 s).

## Findings along the way

- A tap reached nothing at first (`tr-2`: no `[winios] drain` line): outside desktop mode
  only a thread that polls its messages drains the input ring, and a launcher waits in
  `GetMessage`. Until the first frame waiting threads now wake every 16 ms
  (`message_ios.c`, as desktop mode does); the tap then drained
  (`[winios] drain type=0 x=956 y=331 flags=0x8003`) and Play worked.
- Play straight from the launcher (`tr-3`) showed the game's error box on the phone:
  `[msgbox] ... caption L"Tomb Raider Error" text L"Failed to initialize Direct3D with current
  settings"`. The prefix's `Software\Crystal Dynamics\Tomb Raider\Graphics` kept
  `FullscreenWidth` 1564 x 720 from earlier plays at the phone's shape; the launcher saved it
  back as it was, and exclusive fullscreen at 1564x720 on a 1280x720 screen failed
  (`-nolauncher` had started anyway). Ok in the launcher's Options wrote 1280 (`0x500`);
  every Play since reached the menu. A person who changes the screen shape gets the same
  box and the same fix, as on Windows after a monitor change.
- The iOS touch controls overlay was absent in these Hollow Knight runs; it was also absent
  in `tomb-raider/main-merge-hk` on the unchanged 6ad89e8 IPA (00:02 on the same night), so not this
  change. Not investigated.
- The dialog's close glyph draws as "r" (no Marlett font in the prefix).

## Not checked

The release app on the phone (decision 0068); a dialog after a game's first frame (not
shown, by design); text entry; other titles' launchers.

## Dialogs after the first frame (PLA-153, decision 0076)

[PLA-153](https://linear.app/playportdev/issue/PLA-153),
[decision 0076](../decisions/0076-guest-dialogs-over-a-running-game.md); `patches/madeira-winios`
0005, `patches/madeira-unix` 0109 (comments only), the app's `GuestWindows`, `GuestPadGate`
(held by a dialog) and the hold after the game's end. Dev IPA
`e83de2458f8a1dc46817ff076b9dd6329fb58f5c4bf4e76c7f5a544cef9aa134`, installed in place; run
directories under `$PLAYPORT_BUILD/agent-notes/pla153/`.

**The reproducer.** Tomb Raider's "Failed to initialize Direct3D" box (`tr-3` above) opens
before any first frame (`first window` +3.97 s, no `first frame` in that run), so it is 0075's
case, not this one. No installed title was known to open a box mid-game, so the reproducer is
a short program, [`dialog-test.c`](2026-10-11-gdi-windows/dialog-test.c) (llvm-mingw, x86-64,
D3D11), put into the container's `Documents/Import` and added through the UI (`pp ui --action
"import:Import/Dialog test"`, the Local title `dir-dialog test`, DXMT, the phone's shape): a
full-screen window presents blue for 8 s, then its own thread opens a message box owned by
it and waits; after OK it presents green for 6 s, then another thread opens an error box
and the program exits with code 3 four seconds later, the box still open. Under desktop Wine
(xvfb) it reaches its first box.

- **`dlg-pad`** (the scripted controller): `first frame` +4.33 s, then
  `[msgbox] ... caption L"Dialog test" text L"This box opened after the first frame. ..."`,
  `[guest-window] hwnd=0x1003c shown: 317x107 at (623,306)`, `showing desktop (623,306 317x107)
  in (0,56 912x308) pt`, `a dialog is shown over the game`, `title: +12.38 s dialog shown`.
  The screenshot is the box (icon, text, OK) across the screen's width over the blue game.
  A (`hostio: window keys for a`): `dialog closed` +29.17 s, `[msgbox] returned 1`, and the
  screenshot after it is the green picture with no box: the game went on. The error box:
  `dialog shown` +35.21 s; `title: done ... exit=0x00000003 after_s=36`; `the game ended with
  a window on screen; it stays until a tap or a button, 60 s at most`. Screenshots 3 s and 15 s
  after the exit show the error box over the last red frame; with no press the restart came
  60 s after the exit, and the library showed "Dialog test stopped unexpectedly / It exited
  with code 0x00000003".
- **`dlg-tap`** (a finger): `first frame` +3.24 s, `dialog shown` +11.31 s; `tap:0.507:0.70`
  (the OK button on the `dlg-pad` screenshot) `on a guest window`, then `dialog closed`
  +33.36 s and `[msgbox] returned 1`; the error box `dialog shown` +39.41 s, exit code 3 after
  40 s, the stop screenshot the held error box.
- **`dlg-hold`** (a press ends the hold): A on the first box (`dialog closed` +29.54 s), the
  error box at +35.59 s, exit after 36 s; A about 10 s into the hold (`vpad: step 1/2
  (script 2)`) was followed at once by `restart: asking CoreDevice to restart`, and the
  screenshot 8 s later is the library with the exit notice.
- Regressions, `--until first-frame+30 --shot`: Hollow Knight **`reg-hk`** `first frame`
  +8.84 s (+9.92 s on `2b2eeb5e…`), main menu; Portal 2 **`reg-p2`** +5.78 s (+5.87 s), main
  menu. In both the only guest-window line is `[guest-window] the game presents: 0 window(s)
  were shown; from now on only windows made after this (dialogs)`: no window tracked, shown or
  copied after it, no `dialog shown` mark.
- Tomb Raider **`reg-tr2`** (Vulkan, 16:9, no arguments, the setting run `reg-tr2-set`): the
  launcher (`first window` +4.12 s), `tap:0.828:0.437` Play, `first frame` +27.37 s, the main
  menu (Continue, New Game, ..., Quit Game; Coastal Forest) at first frame+10, no dialog
  after the first frame. Its own window was shown for a moment before the first frame (0075's
  known flash), as before.
- `pp test` passed (HostIOKit's new `testADialogAndTheMenuHoldTheGuestUntilBothAreGone`);
  `pp build` passed 80 IPA checks; `pp check --variant release` compiles.

**Along the way.** A first Tomb Raider run (`reg-tr`) played with the app's stored settings:
the arguments `-nolauncher` and the phone's shape (1564x720), set outside this work. It showed
the "Failed to initialize Direct3D" box before any frame (0075's case, as in `tr-3`). With the
settings above it reached the menu.

**Not checked:** the release app on the phone (decision 0068); a real title's dialog after
its first frame (none known among the installed titles); a dialog made before the first
frame and shown after it (by design not shown); how much the 16 ms polling costs while a box
is up (not measured; it stops with the last box by code, not by a measurement).

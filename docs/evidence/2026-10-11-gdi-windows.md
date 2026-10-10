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

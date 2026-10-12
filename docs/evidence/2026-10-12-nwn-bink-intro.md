# Evidence: NWN Diamond's Bink intro is GDI; the guest windows now give way at the first frame shown (PLA-236)

**Date:** 2026-10-12. **Kind:** a read of earlier phone logs and of the game's imports, Wine's
ddraw/wined3d/opengl32 sources, phone runs (dev app). **Linear:**
[PLA-236](https://linear.app/playportdev/issue/PLA-236), title
[PLA-187](https://linear.app/playportdev/issue/PLA-187) (Neverwinter Nights Diamond, GOG
1207658890, i386, OpenGL through Zink). **Changes:** `app/Sources/S1Probe/HostIO.swift`
(`PacedMetalLayer.onFirstPresent`), `app/Sources/S1Probe/LaunchCoordinator.swift`. The owner was
away, so the conservative option was taken at each choice below. Run directories are under
`$PLAYPORT_BUILD/ui-runs/`, scratch under `$PLAYPORT_BUILD/agent-notes/sweep11e/`.

**IPAs:** before, `Playport-26.5-13f8dda9.ipa` (sha256
`13f8dda954c39a5e3ea5999ecd457015b97ccef906984b8837485d475b1b43f7`; runs `20261012T170848`,
`20261012T171052` of [2026-10-12-gog-inno-ansi-code-page](2026-10-12-gog-inno-ansi-code-page.md));
after, `Playport-26.5-895abae8.ipa` (dev, sha256
`895abae8d4df13e7678aea400114726655fe2c57c391e125d1def67ce984a85b`, dfca784 with this change),
installed in place.

## What the intro does

From the log of `20261012T171052` (black through first frame +75 s):

- The game plays six Bink movies in turn (the folder has `AtariLogo`, `BiowareLogo`,
  `WOTCLogo`, `fge_logo_black`, `NWNintro`, `XP1_Intro`, `XP2_Intro`, …). Each makes an
  `NWN Movie Player Window` with no window surface of its own (`[win-pos] … surface=0x0`): a
  child of the game window (`Neverwinter Nights v1.69.8109`, hwnd `0x30058`, 800×600, the
  OpenGL window). They last about 1, 4, 8, 8 and 6 s, and the sixth runs on: the movies play in
  real time, so nothing waits on an audio clock.
- `nwmain.exe` imports Bink's buffer API (`BinkBufferOpen`, `BinkBufferLock`/`Unlock`,
  `BinkCopyToBuffer`, `BinkBufferBlit`, `BinkGetRects`). Bink opens DirectDraw for each movie
  (wined3d on Zink: a `WineD3D fake window`, a `wined3d_cs` thread, a context on the movie
  window, and at the release ddraw's `Hidden D3D Window`), but the frames reach the screen
  through GDI: one `[put-image]` (dibdrv `PutImage`) per frame, 640×480 at (80,60) of the 800×600
  window, 403 in the run.
- No OpenGL drawable presents during the movies: every `takes layer … (never presented)`, and
  fewer than 16 drawables are taken in 75 s (no `[frames]` line). The `first frame` mark at +5.5 s
  is a drawable Zink took when wined3d made its first context current, not a frame shown.
- That mark called `GuestWindows.gamePresents()`, so the game window, whose surface the GDI frames
  were painted into, became "the game's" (decision 0076) and was never copied again.
  Before the first frame Playport shows such a window (0075). So the frames were drawn and
  never shown.

## The change

Decision 0075 ends the guest windows "once the game's swap chain presents"; the app ended them
when the game first *asked* for a drawable. `PacedMetalLayer` now has a second one-shot,
`onFirstPresent`, run from the drawable's presented handler when its `presentedTime` is above 0.
`LaunchCoordinator` calls `GuestWindows.gamePresents()` there, not at the `first frame` mark, and
resets the handler where it resets the first-frame one. The `first frame` mark, the launch screen
and its steps are unchanged. **Choice recorded:** the narrow change, the guest windows' cut-over
only, not moving the `first frame` mark, which `pp ui --until`, `pp perf` and the launch screen
rely on. No decision record: the implementation now does what 0075 says.

## Phone runs on `895abae8`

| Run | What | Outcome |
| --- | --- | --- |
| `20261012T192219` | NWN Diamond: Play, wait 15 s, then six taps 4 s apart, a screenshot after each action | first frame +5.65 s; `first window` +5.90 s (the game window, shown with the movie). Screenshots show the movies in the game window, aspect-fitted (a logo's black frame at +15 s; the intro's forest after the first tap). After the next taps, the main menu (New, Load, Multiplayer, Movies, Options, Exit), drawn by OpenGL. The next tap opened Load Game. `[guest-window] the game presents` came at the menu's first present (19:23:20), 45 s after the first frame. The run ended at `--wait 120` with the game still running (no stop condition was given) |
| `20261012T192502` | Hollow Knight (`app-367520`), first frame +10, screenshot | first frame +9.7 s, main menu; `the game presents` follows the first frame |
| `20261012T192557` | Portal 2 (`app-620`), first frame +10, screenshot | first frame +5.6 s, the Source logo |

The pad test asked for is answered by the same run: taps (the touch path's left button, on the
shown window) skip the movies, and the menu appears. **Choice recorded:** no separate pad run on
the old IPA, to keep to the testing budget. NWN uses no XInput, so taps were used rather than the
scripted pad's buttons.

## Limits

- On the guest windows layer the movie is a copy of each GDI flush, and so is its frame rate.
  Smoothness was not measured. Neither was audio sync.
- Gameplay past the menu (character creation, a module) was not played.
- For every title the guest windows now end a presented frame later than before (about one
  display refresh). A title whose drawables are never reported presented keeps its windows layer
  and the 60 Hz polling of decision 0075 for the session; HK and Portal 2 report it at once.

## Status

NWN Diamond goes from **Broken** to **Playable with issues** in `site/compatibility/games.json`:
it reaches its menu and answers taps, but nothing past the menu was played. **Choice recorded:**
not Works until a module is played.

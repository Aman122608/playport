# 0097: A game's display mode change is emulated on the virtual monitor

**Status:** accepted, 2026-10-12, by the compatibility speedrun's worker with the owner away (for
the owner's review). The conservative option was taken at each choice, as recorded below.
[PLA-197](https://linear.app/playportdev/issue/PLA-197).
Amends [0088](0088-opengl-window-surfaces-and-pacing.md)'s "Mode changes are not emulated".
[Evidence](../evidence/2026-10-12-display-modes.md).

## Context

The iOS port registers no display source. Every process sees one synthesized virtual monitor,
`\\.\DISPLAY1`, at the title's screen size (`MADEIRA_SCREEN_W`/`H`). A `ChangeDisplaySettings`
that named `\\.\DISPLAY1` failed with `BADPARAM` (`find_source FAILED`). One with no name
answered "successful" and changed nothing. GoldSrc's software renderer (Counter-Strike and
Half-Life `-soft`) asks for 640x480 by name. It then gave up ("The specified video mode is not
supported") and faulted in its own unloaded window procedure. Under desktop Wine and Proton the
change succeeds. win32u's mode emulation lists the common sizes at 32, 16 and 8 bpp and reports
the size set as current, while the host keeps its real mode.

## Decision

- **Mode list.** The virtual monitor lists 640x480 and 800x600, the 4:3, 16:9, 16:10, 5:4 and 5:3
  sizes of Wine's emulation that fit in the native size, and the native size. Each is listed at
  32, 16 and 8 bpp, deepest first and smallest first, as win32u sorts them. The refresh rate is
  the panel's, as before.
- **Mode change.** A change to a listed mode, naming `\\.\DISPLAY1` or the primary, is accepted
  (`patches/madeira-unix` 0125). Unless the call passes `CDS_TEST` or `CDS_NORESET`, it becomes
  the current mode:
  - the monitor's rect, `SM_CXSCREEN`/`SM_CYSCREEN` and `ENUM_CURRENT_SETTINGS` report the new
    mode;
  - the desktop window is resized;
  - `WM_DISPLAYCHANGE` is broadcast.

  A change with no mode, or one back to the native size, returns to native.
- **Conservative choices:**
  - the registry mode stays native, so a mode never outlives the session;
  - the frequency is not checked;
  - an unlisted mode named on `\\.\DISPLAY1` is refused with `BADMODE`, as Windows refuses it;
  - an unlisted mode for the unnamed primary keeps its earlier answer: successful and
    unchanged.
- **Presentation.** Nothing in the app changes. The real surface stays the panel's. A game
  window of the mode's size has a swap chain of that size, and the layer aspect-fits it with bars.
  A 640x480 game shows 4:3 whatever the aspect setting, as every swap chain smaller than the
  screen already does (0073). An OpenGL window's swap keeps the layer's `drawableSize` at the
  window's size (madeira-unix 0126). Zink acquires a window's buffer before a copy reads it
  (`patches/mesa` 0034). Both were needed for GoldSrc's resize after its mode change.

## Limits

- A mode set by a process that exits without restoring it stays current until the session ends.
  Windows would restore it then. Playport plays one title per process (0030), so the next title
  starts native.
- Only the size is emulated. Screen DCs stay 32 bpp whatever bpp is set.

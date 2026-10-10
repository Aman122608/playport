# 0075: The guest's GDI windows until the game's first frame

**Status:** accepted, 2026-10-11, by the supervisor of the compatibility speedrun with the
owner away (for the owner's review);
[PLA-152](https://linear.app/playportdev/issue/PLA-152), which blocks
[PLA-151](https://linear.app/playportdev/issue/PLA-151) (Tomb Raider's launcher).
Changes what the title's screen shows before the first frame (the launch screen of
`docs/ARCHITECTURE.md`, "Launch").

## Context

A game's swap chain presents into the app's one full-screen Metal layer. A window Windows
draws with GDI (a launcher, a setup page, a message box) had nowhere to appear: Madeira's
window surfaces and compositor run only in its `MADEIRA_DESKTOP=1` mode, whose view covers
the game's layer and gives no first-frame signal. Tomb Raider without arguments sat alive
under the launch screen with its launcher open (PLA-151), and a dialog before the first
frame was readable only from `[win-text]` log lines
([launcher feasibility](../evidence/2026-10-02-launcher-feasibility.md)).

## Decision

- **Every top-level window gets a GDI surface the app sees** (`patches/madeira-unix` 0109):
  Madeira's winios surfaces and window frames in every mode, not only desktop mode.
- **Until the game's swap chain presents, the app shows them** (`patches/madeira-winios`
  0004, `GuestWindows` in `HostIO.swift`): each top-level window's last picture, placed as
  on the Windows desktop, in a layer over the game layer. A window is shown while it is
  visible (shown, not minimised), at least 32×32, and its client area is not one colour: a
  game's own window, erased to one colour until its swap chain draws, stays hidden and keeps
  the launch screen up. The window shown last is on top. The bounding rect of those shown is
  aspect-fitted to the screen, so a launcher fills it and a dialog's drop-down stays inside.
- **The first window shown ends the launch screen**, as the first frame does: a `first
  window` mark (`pp ui --until mark:first window+S`) and launch step; the `first frame` mark
  is unchanged and still comes from the game layer.
- **Input:** a finger is the left button at the desktop pixel under it. A controller's
  presses are keys a dialog answers: the D-pad and left stick the arrows, A Enter, B Escape,
  X Space, RB Tab, LB Shift+Tab (HostIOKit `WindowKeys`); the pads still reach XInput. Until
  the swap chain presents, threads waiting for messages wake every 16 ms to take the queued
  input, as Madeira's desktop mode does: a launcher waits in `GetMessage`, which drained
  nothing before.
- **Once the swap chain presents, its picture is the screen**: the layer is removed, no
  window is shown again, no surface is copied, and waiting threads wait once again. A dialog
  a game opens after its first frame stays invisible (an overlay over a running game, with
  its z-order and input, is a separate decision).
- Desktop mode (`MADEIRA_DESKTOP=1`) is unchanged. No setting or launch mode: this is how
  every title starts, in the dev and release apps (decision 0012).

## Costs and limits

- Before the first frame every top-level window's GDI flush is copied once to the app; the
  pictures are upscaled bitmaps (no high-resolution text), and a layered window's
  transparency shows as black.
- Waiting guest threads wake 60 times a second until the first frame; a title that never
  presents (a GDI-only game) keeps that for its whole session.
- Text entry has no on-screen keyboard; there is no pointer for hover, right click or drag
  beyond a finger's move; owner-drawn launchers work only as far as they take mouse or keys.
- A game window briefly painted with detail before its swap chain (Tomb Raider's, for a
  moment) is shown until the first frame.

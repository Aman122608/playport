# 0076: The guest's dialogs over a running game

**Status:** accepted, 2026-10-11, by the supervisor of the compatibility speedrun with the
owner away (for the owner's review);
[PLA-153](https://linear.app/playportdev/issue/PLA-153), the follow-up to
[PLA-152](https://linear.app/playportdev/issue/PLA-152). Amends
[0075](0075-guest-windows-before-the-first-frame.md): its rule that after the first frame
only the swap chain shows now has the exceptions below; the rest of 0075 stands.

## Context

Under 0075 a message box or a dialog a game opened after its first frame was alive but
invisible: the game's thread sat in the box's modal loop, waiting in `GetMessage` (which
drained no input once the polling stopped), behind a frozen picture. An error box shown
just before a game ends is the common case; on the phone it ended in the restart's
"exited with code" notice, with the game's own words only in `[msgbox]` log lines.

## Decision

- **The game's windows are those it has at its first frame** (`patches/madeira-winios`
  0005). Every top-level window that had a frame before the swap chain first presents is
  the game's: never shown or copied again, so the game's own window is never drawn over its
  swap chain. So is a window made later that covers the whole desktop (a game window made
  again).
- **A window made after the first frame is a dialog** (a `MessageBox`, a `#32770` dialog, an
  owned popup, any other new top-level window). While it is visible and at least 32×32 its
  surface is copied; once its client area is not one colour it is shown over the game as
  0075 shows windows before the first frame (placed as on the desktop, the bounding rect of
  those shown fitted to the screen, the one shown last on top). The layer goes back on the
  game layer only while one is shown, and leaves it when the last goes.
- **Input goes to the dialog while one is up.** A finger is the left button at the desktop
  pixel under it; a controller's presses are its keys (0075's `WindowKeys`). The guest's
  XInput pads are at rest meanwhile, and a button still down when the dialog goes stays up
  for the game until it is let go (HostIOKit `GuestPadGate`, held by the in-game menu or a
  dialog). When the last dialog goes, the game has its touches and pads again.
- **Waiting threads poll only while a dialog is visible.** `winios_windows_polling` is 1
  until the first frame (0075) and again while such a window is visible, so the box's modal
  loop drains the touches and keys queued for it; otherwise threads wait as before. With no
  dialog nothing is copied and nothing polls.
- **Marks:** `dialog shown` and `dialog closed` (`pp ui --until "mark:dialog shown+S"`),
  and `[guest-window] a dialog is shown over the game` / `no dialog over the game`.
- **The last window stays when the game ends with it open.** A game that exits while a
  window is still on screen (an error box from another thread, or a process ended with its
  box up) leaves that picture on the title's screen until a tap or a controller button, a
  minute at most; then Playport restarts as before (decision 0029), with its notice. A box
  the player dismissed before the game ended is not shown again.
- Desktop mode (`MADEIRA_DESKTOP=1`) is unchanged. No setting or launch mode (decision
  0012): dev and release apps alike.

## Why

The window a dialog appears in is the one Windows would show over the game, and 0075's
layout already makes a small box readable and its buttons large enough to tap. Taking the
windows present at the first frame as the game's is the conservative test: Winios sees only
handles and rectangles, and a game window must never cover its own swap chain, so a
dialog the game made before its first frame and shows later stays hidden (a known gap,
below). The polling and copying follow a dialog's life, so a game without one runs as it
did under 0075.

## Costs and limits

- A window the game created (hidden) before its first frame and shows later is not shown.
- A new window that is not a dialog (a second game window smaller than the desktop) makes
  waiting threads poll and its flushes copied while it is visible; it is shown only if its
  client area has more than one colour.
- The fitted box covers the middle of the game; the game's picture behind it does not
  change while its thread waits in the box.
- Touches anywhere go to the dialog while one is up (outside the box they reach the
  desktop pixel under them, usually the disabled owner).
- The hold after the game's end delays the restart by up to a minute when nobody is there.
- 0075's limits stay: bitmaps upscaled, layered windows black where transparent, no text
  entry, the close glyph drawn as "r".

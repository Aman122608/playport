# Messages sent to the desktop window (PLA-252, PLA-249)

**Linear:** [PLA-252](https://linear.app/playportdev/issue/PLA-252) (fixed),
[PLA-249](https://linear.app/playportdev/issue/PLA-249) (Tomb Raider: Anniversary, GOG 1202885143, now
starts). Sweep 2.11o of [PLA-112](https://linear.app/playportdev/issue/PLA-112). iPhone18,4; runs under
`$PLAYPORT_BUILD/ui-runs/`. Follows [2026-10-13-small-shared-sections](2026-10-13-small-shared-sections.md),
after which the game still opened no window.

## Symptom

`tra.exe` (i386) loaded its DLLs, GOG's dxwrapper and DEPHook, DXVK's D3D9 (it lists the adapter),
`uxtheme.dll` and the builtin classes, then waited with low CPU and never made a window: no `[win-text]`
line at all (not even `Default IME`), no `[guest-window]`, black screenshots at 25 and 60 s (run
`s11n2-tra`, IPA `1ac38777…`). Its one thread sat in `wait_for_multiple_objects`, woken about 60 times a
second to drain the input ring. The `mprotect_exec` and `Handled inline self-modifying code` lines were
DEPHook.asi writing its own data on a host page it shares with its code: handled, not the stall.

## Desktop

Wine 11.18 in an owned Xvfb, a scratch prefix, the game files pulled from the phone and the install
script's registry values (decision 0103):

- Without the `bigfile.*` archives the game shows its failure box (`ERROR OUT OF TABLE RANGE`).
- With them it shows its **Rendering Setup** dialog (display adapter, resolution, effects; Ok, Cancel)
  within a few seconds.
- A relay trace from `tra.exe`: `InitCommonControls`, `GetDesktopWindow`, then
  `DialogBoxParamW(0x400000, 0x66, <desktop window>, …)`. user32 disables a modal dialog's owner before
  it creates the dialog: `NtUserEnableWindow(<desktop window>, FALSE)` sends `WM_CANCELMODE` and
  `WM_ENABLE` to the desktop window, and explorer's desktop thread answers them.

## Cause

On the phone no explorer runs. The session root (`app/SessionRoot/playport-session.c`) makes the
desktop window (madeira-unix 0043); wineserver detaches its thread but the window's handle entry
keeps the session root's thread id, so win32u sends the desktop window's messages to that thread. It
waited for the title with `WaitForSingleObject` and never read its message queue, so the title's
`SendMessage` waited for ever: before the dialog's `CreateWindowEx`, which is why no window existed.

## Fix

The session root waits for the title with `MsgWaitForMultipleObjects(…, QS_SENDMESSAGE)` and calls
`PeekMessageW(PM_QS_SENDMESSAGE)` when a message was sent, as explorer's desktop thread does. The
desktop window has no window procedure in the root, so each sent message is answered with 0 at once.
It logs `session: title N: answering messages sent to the desktop window` the first time. No upstream
code changed.

## Phone (IPA `07a0c124db0e383d8e9088ef7e76f131b91b85820c652c9a8107cd2eeb6e97e0`)

| Run | What | Result |
| --- | --- | --- |
| `s11o-tra` | TRA Play, Vulkan 16:9, `--until "mark:first window+10" --shot` | `answering messages sent to the desktop window`; first window +3.52 s; `[win-text]` `Tomb Raider: Anniversary - Setup` and its labels; `[guest-window] hwnd=0x10040 shown: 432x392`. The screenshot shows the setup dialog, readable (adapter, 640 by 480, 120 Hz, effects, Ok/Cancel/Set To Default) |
| `s11o-tra-ok` | TRA Play, `wait:10`, `tap:0.303:0.906` (Ok), `--until first-frame+30 --shot` | first window +3.46 s, first frame +10.88 s, until reached. Screenshots: the Eidos logo (+25 s), the intro movie (Bink, pillarboxed 4:3 at 640x480) at +60 s and at the stop |
| `s11o-hk` | Hollow Knight gate | first frame +9.64 s, main menu |
| `s11o-p2` | Portal 2 gate | first frame +5.83 s |

Not checked: the main menu and gameplay after the intro, pad input in the dialog, and whether the
dialog shows again once Ok was pressed (it showed at the second start; the first was ended before
Ok). A tap on Ok goes through the guest-window touch path (decision 0075).

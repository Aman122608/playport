# Soccer Online: Ball 3D: a 0x0 fullscreen window from the virtual monitor's name

**Date:** 2026-10-09. **Branch:** `dev/compat-speedrun`. **Linear:** PLA-53.
**Title:** Soccer Online: Ball 3D (Steam `485610`), x86-64, Unity 5.5.3f1,
Direct3D 9 (the player's default; `-force-d3d11` fails the same way).

**Result:** the game draws its tutorial pitch on Vulkan at 16:9, first frame at
5 s. Before the fix it was alive with no first frame.

## Before

IPA `02fca070f2148ac98a0d35d45618b5d3a1f97b66145289bfa207f45954727342`, Vulkan,
16:9. The 320x240 `[win-pos]` window is not a dialog: `[win-text]` names it
`Ball3D`, class `UnityWndClass`, the player's own window before it takes its
resolution. DXVK never made a Vulkan swap chain (no `Presenter: Actual swapchain
properties`). With `-force-d3d11` the player says
`D3D11: Failed to create RenderTexture (0 x 0 fmt 19 aa 2)`. The game's
PlayerPrefs (`HKCU\Software\Unusualsoft\Ball3D`) held
`Screenmanager Resolution Width/Height` = 0. Unity's
`-screen-width 1280 -screen-height 720 -screen-fullscreen 1` changed nothing.

A trace (`runtime`: `env.WINEDEBUG=+system,+win`) shows the player list the
modes of the monitor that `GetMonitorInfoA` names, then size its fullscreen
window from them:

    NtUserEnumDisplaySettings device L"WinDisc", index 0xffffffff
    AdjustWindowRectEx ((0,0)-(0,0)) 84000000 0 00000000
    NtUserSetWindowPos hwnd 0x10030, after 0xffffffffffffffff, 0,0 (0x0), flags 00000150

## On the workstation

The phone's game files under desktop Proton 10, in a scratch prefix and a 1280x720
virtual desktop, draw the same tutorial. They do so too with the phone's 0x0
PlayerPrefs. The traced sequence is the same, except that the monitor is
`\\.\DISPLAY1` and the window is sized 1280x720:

    NtUserEnumDisplaySettings device L"\\\\.\\DISPLAY1", index 0xffffffff
    AdjustWindowRectEx ((0,0)-(1280,720)) 84000000 0 00000000

The player builds its display list from `EnumDisplayDevicesA`, and on the phone
that list's only adapter is the synthesized `\\.\DISPLAY1`. The window's monitor,
named `WinDisc` (Wine's name for a monitor with no source), was not in it.

## Fix

`patches/madeira-unix` 0101: on iOS the virtual monitor's `szDevice` is
`\\.\DISPLAY1`, the adapter that `NtUserEnumDisplayDevices` synthesizes.
`EnumDisplaySettings` already answered for any name in this regime.

## After

IPA `05707e9e1762f5764e9f6ad084927b39e7c12686b346e61934bd490258ed4db7`, Vulkan,
16:9, no arguments, the same 0x0 PlayerPrefs: `[win-pos]` shows the window go
to 1280x720 (topmost, shown). DXVK's swap chain is 1280x720. The first frame
comes at 5.08 s. At first frame+30 s the screenshot shows the tutorial pitch:
the stadium, the "Movement" W/A/S/D prompt and the player's disc. The title
needs no compatibility default. Screenshots stay in the run directory under `.work`.

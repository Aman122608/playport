# Evidence: the menu, window station and desktop thunks audited for the WoW64 window

**Date:** 2026-10-08. **Phone:** iPhone18,4, iOS 27.0, Wi-Fi, dev build, driven with `pp ui`.
**Linear:** PLA-110 (the fix), PLA-108 (Super Meat Boy), PLA-67 (House of Golf 2, whose
installer stopped at the same call), PLA-12. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.

Checked on the phone with the final dev IPA (wine-pe 0032 and 0033, sha256 below): Super Meat
Boy now runs past window creation, starts Direct3D 9 on DXVK and presents frames for 60 s, but
the screen stays black (its PNG textures fail to load: [Next blocker](#next-blocker)). Hollow
Knight and Portal 2 reach their first frames as before.

## The stop

wine-pe 0018 lets a windowed i386 process run only the wow64win thunks listed in
`dlls/wow64win/window_audited.h`: those whose every pointer argument and returned pointer
converts through the guest window, and whose values kept for the guest stay guest values. Any
other win32u call ends the process. After madeira-unix 0099 Super Meat Boy (Epic `Buffalo`)
logged "Creating Window", then
([2026-10-08-wow64-opengl32.md](2026-10-08-wow64-opengl32.md), run `20261008T195309`):

```
002c:err:wow:window_unaudited Playport: i386 win32u system call 15d0 (args …: 00010034 ffffffff 00000400 00000001); its thunk is not audited for the guest window, stopping
```

`15d0` is `NtUserThunkedMenuItemInfo`, method 1 (`NtUserInsertMenuItem`), `MF_BYPOSITION`,
position -1: an `AppendMenu`. The title exited `0xc00000bb`.

## The audit

**wine-pe 0032** (`wow64win: audit the menu item, menu info and menu bar thunks for the window`):

- `NtUserThunkedMenuItemInfo`, every method against win32u's `menu.c`:
  - insert and set: win32u's `set_menu_item_info` reads `dwTypeData` as the item's text only
    under `MIIM_STRING`. The thunk passed `UlongToPtr` (an unconverted guest address); it now
    converts it through the window there. Otherwise the value (a bitmap handle that user32's
    `MIIM_TYPE` normalizing leaves) passes as it is; win32u never dereferences it.
  - `GetMenuItemInfoA/W`: `get_menu_item_info` writes the text into `dwTypeData` under
    `MIIM_TYPE` or `MIIM_STRING`, and under `MIIM_TYPE` may replace it with the item's bitmap
    handle or 0. The buffer now converts in; on the way back the guest gets its own address
    when win32u left the buffer, or the handle or 0 it put there (before: the host pointer
    truncated).
  - `CheckMenuRadioItem` passes two integers in `cch` and `fMask`; `GetMenuState`,
    `GetMenuItemID`, `GetSubMenu`, `GetMenuDefaultItem` take no block.
  - The info block and the unused `UNICODE_STRING` argument already converted (0018); the other
    fields are handles, ids and item data the guest gets back as it gave them.
- `NtUserThunkedMenuInfo` (`SetMenuInfo`) and `NtUserGetMenuBarInfo`: only their converted
  top-level block, handles and plain data. No change.

**wine-pe 0033** (`wow64win: audit the window station and desktop creation thunks for the
window`): `NtUserCreateWindowStation`, `NtUserCreateDesktopEx` and `NtUserOpenDesktop` take
their object attributes through `objattr_32to64`, which converts the name, security descriptor
and quality of service through the window (0018), as the audited `NtUserOpenWindowStation`
does. `CreateDesktopEx`'s device string converts through `unicode_str_32to64` and its
`DEVMODEW` (no pointers) through `get_ptr`. No change.

The menu thunks already audited by 0018 were rechecked and need nothing: `NtUserCreateMenu`,
`CreatePopupMenu`, `SetMenu`, `DrawMenuBar`, `DrawMenuBarTemp`, `DeleteMenu`, `RemoveMenu`,
`DestroyMenu`, `TrackPopupMenuEx`, `GetMenuItemRect`, `MenuItemFromPoint`, `GetSystemMenu`;
`GetMenuInfo` is `NtUserCallTwoParam`, converted in 0018.

Left unaudited (still stop the process): the clipboard data thunks, `NtUserGetPointerInfoList`,
the D3DKMT allocation, keyed-mutex, resource and synchronization-object thunks,
`NtGdiDrawStream`, `NtGdiStartDoc`, `NtGdiGetSpoolMessage`. Each has nested buffers or
handles from native allocations; none was reached here.

Host test: `build/wow64/menu_item_test.c` (run by `build/wow64/test.py`, in `pp test`) compiles
the patched `wow64_NtUserThunkedMenuItemInfo` from `user.c` with 0018's helpers over a mock of
win32u's reads and writes: `AppendMenu`'s insert at -1, a separator, a `MIIM_TYPE` bitmap,
item data, a text get, a truncated get, a bitmap and a separator under `MIIM_TYPE`, a length
query, a get without text, `CheckMenuRadioItem`, the methods without a block, and no window.
With the conversion taken out, the test faults in the thunk.

## Runs

| Run | IPA | JIT | First frame | Outcome |
| --- | --- | --- | --- | --- |
| `20261008T201259` | `734267e3` (0032) | 2.28 s | none | Super Meat Boy: past the menus; stopped at the unaudited `NtUserCreateWindowStation` |
| `20261008T203141` | `8e37c251` (0032, 0033) | 2.42 s | +3.83 s | Super Meat Boy to `first-frame+20`: alive, presenting; screen black |
| `20261008T203234` | `8e37c251` | 2.38 s | +9.71 s | Hollow Knight to `first-frame+10`: main menu |
| `20261008T203324` | `8e37c251` | 2.57 s | +5.90 s | Portal 2 to `first-frame+10`: the "powered by Source" splash |
| `20261008T203604` | `8e37c251` | 2.50 s | +3.78 s | Super Meat Boy to `first-frame+60`: alive, presenting; screen black |

`20261008T201259` (`./pp ui --action play:epic-Buffalo --until first-frame+20 --shot`):
"Creating Window", the `AppendMenu` calls pass, the window `Super Meat Boy v1.2.5` is placed
(`[win-pos] #4 hwnd=0x10030 … vis={462,120,1110,627}`), then

```
002c:err:wow:window_unaudited Playport: i386 win32u system call 136d (args …: 00f8fa74 0000037f 00000000 00000000); its thunk is not audited for the guest window, stopping
```

`136d` is `NtUserCreateWindowStation` (`WINSTA_ALL_ACCESS`); 0033 audits it. Exit
`0xc00000bb` 253 ms after the game started.

The other four runs were one locked session (install, then the three plays) and one more play,
all with the final dev IPA. Installed in place; the app's container and titles were kept.

- Super Meat Boy (`--action play:epic-Buffalo`), `20261008T203141` and `20261008T203604`: no
  `window_unaudited` line. The game logs (through `OutputDebugStringA`) "Window Created",
  "Resource Pool Created", loads its shaders, `gamedata.dat` and `gameaudio.dat`, runs its
  "Initial Render Test" and then loads fonts, menus, animations and sounds. DXVK 3.1.1 creates the
  Direct3D 9 device and swap chain (`kk-wsi` presents at 640x480, then 1564x720); the presenter
  counts 1824 frames by `first-frame+20` and 4368 by `first-frame+60`, and the app ends the title
  at `--until` (no crash, no `crashes/`). Both screenshots are an entirely black screen.
- Hollow Knight (`--play app-367520`), `20261008T203234`: the main menu (title, Start Game,
  Options, Achievements, Extras, Quit Game).
- Portal 2 (`--play app-620`), `20261008T203324`: the "powered by Source" splash with Valve's
  copyright text.

## Next blocker

Every texture the game loads from a PNG fails (50 times in each run):

```
002c:err:wow:Wow64SystemServiceEx Playport: i386 system call 00b6 (args 00f8f6c8 00020019 00f8f5e4 00000000) failed c0000034 [199]
002c:fixme:d3dx:D3DXCreateTextureFromFileInMemoryEx Unrecognized file format, returning failure.
```

The first is "Creating Texture From File: resources/Fonts/EFIGS_0.png", just after i386
`windowscodecs.dll` loads. Wine's `d3dx9_43` decodes PNG through WIC, and i386 system call `00b6`
is `NtOpenKeyEx` (`KEY_READ`) failing with `STATUS_OBJECT_NAME_NOT_FOUND`. The log does not name
the key. The likely cause, not yet confirmed: WIC finds its decoders through
`HKCR\CLSID\{7ED96837-96F0-4812-B211-F13C24117ED3}\Instance`, which an i386 process reads
from `Software\Classes\Wow6432Node`, and the IPA's seed `registry/system.reg` has no
`Wow6432Node` key at all (the 64-bit `Software\Classes\CLSID\{389EA17B-…}`, WIC's PNG decoder,
is there). This is a prefix registry gap, not a WoW64 thunk; it was not fixed here. Confirming it
(the key name, under desktop Wine or with a registry trace) is the next step.

IPAs built from this tree (wine-pe 0032 and 0033), both verified:

- dev `Playport-26.5-8e37c251.ipa`, sha256 `8e37c25191ae8bab0b18fde80da8612d5fd01454a0fb931f0e578418c10d2ac7` (on the phone; the runs above)
- release `Playport-26.5-release-cbb95ec1.ipa`, sha256 `cbb95ec1576bbdb69a0d44820636f45a82aa5af6284175f2524f7bf84be34a62` (not installed: the release app gets no automated phone tests)

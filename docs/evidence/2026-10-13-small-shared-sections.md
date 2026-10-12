# Small writable section views in an i386 process (PLA-307, PLA-248)

**Linear:** [PLA-307](https://linear.app/playportdev/issue/PLA-307) (fixed),
[PLA-248](https://linear.app/playportdev/issue/PLA-248) (BioShock, now reaches its menu scene),
[PLA-249](https://linear.app/playportdev/issue/PLA-249) (Tomb Raider: Anniversary, unchanged).
Sweep 2.11n2 of [PLA-112](https://linear.app/playportdev/issue/PLA-112). iPhone18,4; runs under
`$PLAYPORT_BUILD/ui-runs/`. Follows
[2026-10-13-gog-msvc2005-command-line](2026-10-13-gog-msvc2005-command-line.md).

## Cause

`map_file_into_view_ex unaligned shared mapping … not supported` came from the WoW64 path only. An
i386 child's data section view (madeira-unix 0071, `ios_wow64_map_file_view`) is claimed in its
window as whole 16 KiB host pages, so a 0x1000-byte view's `file_view` is 0x4000 bytes, but it was
mapped with the view's own size. `map_file_into_view_ex` then saw a mapping that ends before its view,
computed a host size of 0x4000 against a map size of 0x1000, and refused a writable view before any
`mmap`; `MapViewOfFile` returned NULL with error 87. A native x86-64 view is exactly the section's size,
so its mapping reaches the view's end and the host's `mmap` rounds it up to the host page itself.

## Fix

madeira-unix 0135: a writable (shared) WoW64 view is mapped over all its host pages, as the host's
`mmap` does for a native view. It stays one `MAP_SHARED` mapping of the section's file, so every view
of a section, and of a duplicated handle, aliases. The bytes past the view's size are the rest of the
section, or zeros past its end within the last host page (the partial last page of a file mapping). The
server still registers the view's own size. Read-only and write-copy views are unchanged (they already
fall back to a copy).

## The probe

`tools/compat/probes/shmap.c`, built x86-64 (`shmap.exe`) and i386 (`shmap-i386.exe`), put in
`Documents/Import/shmap` and `…/shmap-i386` with AFC and imported through the UI (`dir-shmap`,
`dir-shmap-i386`). For sections of 0x14, 0x1000, 0x3000, 0x4000 and 0x5000 bytes, pagefile backed and
file backed: two `FILE_MAP_WRITE` views and a third through a duplicated handle must map; a byte written
through each view at the start, middle and last byte must read back through the other two; for a file,
`ReadFile` after unmapping must see the last bytes written.

| Case | desktop Wine 11.18, 64 / 32 | phone x86-64, before / after | phone i386, before | phone i386, after |
| --- | --- | --- | --- | --- |
| 0x14, 0x1000, 0x3000, 0x5000 (anon and file) | ok | ok / ok | map fails, error 87 | ok (map, alias, file) |
| 0x4000 (anon and file) | ok | ok / ok | ok | ok |

Runs: `s11n2-shmap-before`, `s11n2-shmap-i386-before` (`7c1f3a42…`); `s11n2-shmap-after`,
`s11n2-shmap-i386-after` (`1ac38777…`).

## Titles on `1ac38777e11521abff3477f201c42c37da2591c2a458eaefdeea37a3f7509cb7`

- **BioShock** (GOG 2022341186), Vulkan, 16:9 (`s11n2-bioshock`): first window at +3.95 s, first frame
  at +50.3 s on DXVK (D3D9). At first frame +30 s it shows the main menu's 3D scene, a gramophone on its
  table; no crash, no `unaligned shared mapping` line. FMOD's 0x14-byte semaphore sections map
  (`[wow64-section] … bytes=0x1000 data=1 file=0 … protect=0x4`). Before: c0000005 at 14 s, no frame.
- **Tomb Raider: Anniversary** (GOG 1202885143), Vulkan, 16:9 (`s11n2-tra`): its two 0x1000-byte
  writable views now map (no `unaligned shared mapping` line), but it still opens no window in 75 s
  and the screen stays black, as in sweep 2.11k. That stall has another cause.
- **Hollow Knight** (`s11n2-hk`): first frame at +8.63 s, main menu. **Portal 2** (`s11n2-p2`): first
  frame at +5.81 s.

Mortal Kombat Komplete Edition, the other title that logged the line, is no longer installed on the
phone and was not replayed.

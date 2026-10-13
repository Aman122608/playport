# Evidence: i386 callers fall back from `HKLM\Software\Wow6432Node` to the one view (PLA-177)

**Date:** 2026-10-12. **Linear:** [PLA-177](https://linear.app/playportdev/issue/PLA-177), from
the [PLA-175 audit](2026-10-11-wow64-registry-view.md). **Change:** `patches/wine-unix` 0030
(wine-unix owns `server/registry.c`; its 0003 already changes it). Decisions
[0072](../decisions/0072-seed-wic-registration-64-bit-classes.md) (one registry view) stands; the
owner was away, so the conservative option was taken at each choice below.

## The hazard

wineserver sends a WoW64 caller's lookup through a key's `Wow6432Node` child whenever that child
exists, with no fallback. The prefix has one view and no `Software\Wow6432Node`, so today i386
callers read `HKLM\Software`. Once any program creates `HKLM\Software\Wow6432Node` (an installer or
launcher writing that path), every later i386 lookup under `HKLM\Software` and HKCR looks there and
fails, for the life of the prefix. The PLA-175 A/B showed `ProgramFilesDir`, `.NETFramework`,
DirectX and four `CoCreateInstance` calls failing. Wine's own i386 code then fills the 32-bit tree
with partial copies (`Wow6432Node\Classes`, `Wow6432Node\Microsoft\Windows NT\CurrentVersion\Time
Zones`), which made it worse.

## The change

Upstream already has a fallback for one key: `Software\Classes\Wow6432Node` is `KEY_WOWSHARE`, so
a first-level class name it lacks is looked up, or created, in `Software\Classes`. 0030 does the
same for `HKLM\Software\Wow6432Node`, on iOS (`WINE_IOS`) only. In `key_lookup_name`, a WoW64 lookup
of a name that a key under `HKLM\Software\Wow6432Node` lacks is looked up at the same path under
`HKLM\Software` (`get_software_wow64_mirror`). If the name is there, the lookup continues in the
64-bit key. If it is in neither, it is created there, as `KEY_WOWSHARE` does. The server writes
`[wow64-reg] HKLM\Software\Wow6432Node exists: i386 callers read it, then HKLM\Software for names
it lacks` to stderr once, when the key is loaded from `system.reg` or created.

The choices:

- **At every depth, not only the first level.** Installers write paths like
  `Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\…`. A first-level fallback would still
  hide `Microsoft\Windows`, `Microsoft\DirectX` and the rest behind that partial
  `Wow6432Node\Microsoft`.
- **Keys only, not values.** A key that exists under `Wow6432Node` wins with its own values, as on
  Windows and in upstream's shared classes key. Merging values would change value enumeration and
  queries in the server. That is more than the issue needs (limit below).
- **Absent names are created in the shared view, as with `KEY_WOWSHARE`.** An i386 program writes
  where it would without the key. A 64-bit reader of the plain path still finds what it wrote, and
  the 32-bit tree does not grow with partial keys. The alternative, upstream's create-in-the-32-bit-key,
  was tried first: it put i386 msacm32's `Wow6432Node\Microsoft\AudioCompressionManager` cache and
  the probe's own key in the 32-bit tree, out of a 64-bit reader's sight.
- **HKLM only.** HKCU's `Software\Wow6432Node` and Classes are unchanged (upstream's FIXME covers
  HKCU too). The issue is about HKLM and HKCR.
- **A prefix without the key is unchanged.** The fallback starts only at a key whose parent is
  `HKLM\Software` and which is its `Wow6432Node`. The seed and the owner's hive have none.

## Host A/B (desktop Wine 11.18)

The pinned Wine (11.19) and 11.18 have the same `server/registry.c`, apart from wine-unix 0003.
0030 was applied to a `wine-11.18` export, and its `wineserver` was built for the host with
`-DWINE_IOS`. That server ran under the workstation's Wine 11.18 (Arch) with PLA-175's i686 probe
(`wowprobe32.exe`). Each run used a fresh copy of PLA-175's scratch prefix with the committed seed
`system.reg`. The script `run-ab.sh` and its outputs are under
`$PLAYPORT_BUILD/agent-notes/pla177/`.

| i386 read | seed, either server | seed + `Wow6432Node\OtherVendor\Game` (64-bit `reg add`), stock | same, 0030 | seed + `Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Game32`, 0030 |
|---|---|---|---|---|
| `ProgramFilesDir`, `CommonFilesDir` | found | key not found | found | **not found** (value of the partial 32-bit `CurrentVersion`) |
| `.NETFramework InstallRoot`, DirectX `Version`, `ProductName` | found | not found | found | found |
| CLSID `InprocServer32` (HKCR) | found | not found | found | found |
| CoCreateInstance DirectMusicPerformance, FilterGraph, DirectPlayLobby, XAudio2 2.7 | `S_OK` ×4 | `REGDB_E_CLASSNOTREG` ×4 | `S_OK` ×4 | `S_OK` ×4 |
| i386 `reg query` of the explicit key at its plain path | n/a | found | found | found |
| i386 `reg add HKLM\Software\NewVendor32\Game` lands in | `Software` | `Software\Wow6432Node` | `Software` | `Software` |
| `Software\Wow6432Node` keys left in `system.reg` | none | the key, `Classes`, `Time Zones`, `NewVendor32` | the key only | the key only |
| server log | none | none | `[wow64-reg] …` once | `[wow64-reg] …` once |

With the seed alone, the probe's output was identical on the stock and the patched server. An
i386 `reg add` of the explicit path `HKLM\Software\Wow6432Node\Explicit32\Game` landed in
`Software\Explicit32\Game`, as `KEY_WOWSHARE` would put it. A value added through the explicit path
of an existing 32-bit key (`Wow6432Node\OtherVendor\Game`) landed in that key (`probe-explicit.txt`).

`tools/tests/test_registry_wow64_fallback.py` compiles 0030's `is_hklm_software` and
`get_software_wow64_mirror` from the patch against a small key tree. It checks the mirror at each
depth, the absence of a mirror for 32-bit-only paths, and that the 64-bit keys, HKCU's
`Wow6432Node`, the shared classes key and a deleted key are left alone.

## Limits

- A key that a program created under `Wow6432Node` hides the values of its 64-bit twin from i386
  callers. Example: a 64-bit `reg add` of a deep `Wow6432Node\Microsoft\Windows\CurrentVersion\…`
  path makes a bare 32-bit `CurrentVersion`, which then hides `ProgramFilesDir` (the last column
  above). ntdll's environment, shell32 and msi still give i386 callers the x86 Program Files paths
  (PLA-175). A value-level fallback is a follow-up if a title needs it.
- An i386 write to a new key through an explicit `Wow6432Node` path lands in the 64-bit view. A
  64-bit program reading that `Wow6432Node` path does not find it there; it does find it at the
  plain path, as it does in a prefix without the key today.
- i386 enumeration of `HKLM\Software` (or of a key that exists under `Wow6432Node`) lists only the
  32-bit keys, as upstream and Windows do.

## On the phone

Dev IPA `01bd82347ca37a959b21882ba6d1e8e2df54a97c7d2870a419ffe823c93b3dc3` (commit `6748815`;
it also carries the broad `patches/mesa` 0030 again, identical to 246365a's) was installed in
place. Every run's `installed` or `result` event names it. iPhone18,4, iOS 27.0, on the charger,
unattended:

- **Hollow Knight** (`app-367520`), `--until first-frame+10 --shot`: first frame +9.7 s, title
  menu (`…T002240`).
- **Portal 2** (`app-620`, i386: a `[wow64-pair]` thread), `--until first-frame+10 --shot`: first
  frame +6.0 s, the Source start screen (`…T002354`).
- **Super Meat Boy** (Epic, i386 `Machine=0x14c`), `--until first-frame+10 --shot`: first frame
  +3.9 s, its intro picture (`…T002502`). It loads ole32, combase and `xaudio2_7` (XAudio2 2.7,
  one of the probe's four classes) and opens an audio stream. No `REGDB_E_CLASSNOTREG` in its log.

No run logged `[wow64-reg]`, so the owner's prefix still has no `HKLM\Software\Wow6432Node`. The
server's other stderr lines (`[wineserver]`, `[srv-own]`) reach `s1-host.log`, so the line would
show there if the key existed. The fallback was not exercised on the phone. No existing test creates
the key in a disposable prefix, and none was added: making the key in the owner's prefix would
leave it there.

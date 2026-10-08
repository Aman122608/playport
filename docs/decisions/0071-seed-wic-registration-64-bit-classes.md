# 0071: The registry seed carries WIC's run-time registration, in the 64-bit classes only

**Status:** accepted, 2026-10-08, by the supervisor of the PLA-78 session (option A of
the [evidence](../evidence/2026-10-08-wow64-registry.md)'s three);
[PLA-78](https://linear.app/playportdev/issue/PLA-78),
[PLA-108](https://linear.app/playportdev/issue/PLA-108). Adds to
[0070](0070-wine-modules-full-proton-set.md)'s registry seed; changes nothing else in it.

## Context

Super Meat Boy (i386, Direct3D 9) presented a black screen: every PNG texture failed in
d3dx9 with "Unrecognized file format". Wine's WIC finds its decoders under
`HKCR\CLSID\{7ED96837-96F0-4812-B211-F13C24117ED3}\Instance` (`CATID_WICBitmapDecoders`),
and the seed had no such key in either view. windowscodecs writes it, with its encoders,
converters, metadata readers and writers and pixel formats, from tables in its
`DllRegisterServer` (`dlls/windowscodecs/regsvr.c`), which wineboot runs through wine.inf's
`RegisterDllsSection`. These are not `WINE_REGISTRY` resources, so the seed (0069, 0070),
which applies only those resources, never had them.

The session was asked to seed the i386 modules' registrations into the 32-bit view
(`Software\Classes\Wow6432Node`). Wine's server makes that key shared
(`server/registry.c`, `KEY_WOWSHARE`): an i386 lookup of a first-level class key that
`Wow6432Node` does not have falls back to `Software\Classes`. With no `Wow6432Node\CLSID`,
i386 callers already see every 64-bit class (the `InprocServer32` path
`C:\windows\system32\…` maps to `syswow64` for them); i386 `xaudio2_7.dll` and `mmdevapi.dll`
loaded through `CoCreateInstance` in the same runs. Seeding `Wow6432Node\CLSID` would end
that fallback at the `CLSID` level, hiding from i386 callers every class registered only in
the 64-bit view.

## Decision

- **`app/tools/prefix-registry.py` writes WIC's run-time registration** (`register_wic`):
  categories, decoders with their patterns, encoders, converters, metadata readers (with
  their container patterns) and writers, pixel formats with their channel masks, as
  `regsvr.c`'s `register_*` functions write them, into `Software\Classes\CLSID`.
- **The tables are read from the build's Wine tree**, not copied: `regsvr.c` in
  `$PLAYPORT_RUN/pe/wine` (the patched tree at the pin) and the GUIDs and enum values from
  its generated `wincodec.h`/`wincodecsdk.h`. Only the logic of the `register_*` functions is
  ported; `WIC_REGSVR_SHA256` is the digest of their source, and a Wine move that changes it
  stops `pp registry` and the build's `--check` until the port is checked.
- **Only the 64-bit view.** No `Wow6432Node` key is seeded; i386 callers read the classes
  through Wine's shared-key fallback. `tools/tests/test_prefix_registry.py` fails if
  `Wow6432Node` appears in the seed.
- **Marked for top-up.** Every key `register_wic` gives a value follows the top-up marker,
  so a prefix that already has a decoder's class key (from its `WINE_REGISTRY` script) gets
  the WIC values it lacks; a value the prefix holds is kept.

## Known gaps

- **i386-only registrations.** A class an i386 module registers and no arm64ec module does
  is not in the seed; under Proton it is in `Wow6432Node`. None is known to be needed; it is
  a Proton parity item on PLA-78. Seeding the 32-bit view needs every 64-bit run-time
  registration mirrored there too, or the fallback above is lost.
- **Other `RegisterDllsSection` modules.** Their run-time registration (DirectShow filters,
  DMOs, crypto OID functions, trust providers, …) is still not seeded; the evidence lists
  them. Each is added when a played title needs it, by the same reading of the source where
  the source has tables.

## What it costs

306 new keys and 279 values on 37 existing class keys (the WIC components' own class keys
from their scripts), all under `Software\Classes\CLSID`; no key or value removed or
changed; no new `InprocServer32` or other activation key. The seed's `system.reg` grows by
62.6 KB. The keys and their 790 values are identical to a Proton 11.0 prefix's 64-bit view
of the same components, plus two pixel formats newer Wine's converter registers.

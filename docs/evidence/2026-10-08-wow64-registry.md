# Evidence: WIC's registration in the prefix seed; Super Meat Boy's textures load

**Date:** 2026-10-08. **Phone:** iPhone18,4, iOS 27.0, Wi-Fi, dev build, driven with `pp ui`;
unattended. **Linear:** [PLA-78](https://linear.app/playportdev/issue/PLA-78) (Proton parity),
[PLA-108](https://linear.app/playportdev/issue/PLA-108) (Super Meat Boy), PLA-12.
**Decision:** [0072](../decisions/0072-seed-wic-registration-64-bit-classes.md). Run directories
are under `$PLAYPORT_BUILD/ui-runs/`.

Follows [2026-10-08-wow64-menu-thunks.md](2026-10-08-wow64-menu-thunks.md), whose next blocker
was Super Meat Boy's PNG textures failing in d3dx9. With WIC's registration in the seed, the
game's textures load (0 "Unrecognized file format", 50 before). The screen is still black at
`first-frame+30`. Hollow Knight and Portal 2 reach their first frames as before.

## The missing key

The failing lookup is `HKCR\CLSID\{7ED96837-96F0-4812-B211-F13C24117ED3}`
(`CATID_WICBitmapDecoders`), opened by i386 `windowscodecs.dll` for `d3dx9_43`'s first
`IWICImagingFactory::CreateDecoderFromStream`. It was missing from both registry views:

- `D3DXCreateTextureFromFileInMemoryEx` decodes through WIC. `CreateDecoderFromStream` calls
  `find_decoder` (`dlls/windowscodecs/imgfactory.c`), which calls
  `CreateComponentEnumerator(WICDecoder)` (`info.c`). That opens `HKCR\CLSID` (`KEY_READ`),
  then `CLSID\{CATID_WICBitmapDecoders}`, then `Instance`. The log line before the d3dx
  failure is one i386 `NtOpenKeyEx` (system call `00b6`, access `0x20019` = `KEY_READ`)
  failing with `c0000034`. The enumerator then has no decoders and d3dx reports
  "Unrecognized file format".
- The seed (`app/registry/system.reg`) had no key under `{7ED96837-…}`; a Proton 11.0
  prefix (`default_pfx/system.reg`) has eight decoder instances there, in each view.
  windowscodecs writes them, with the encoder, converter, metadata and pixel-format
  categories, from `DllRegisterServer` tables (`dlls/windowscodecs/regsvr.c`), which wineboot
  runs through wine.inf's `RegisterDllsSection`. They are not `WINE_REGISTRY` resources,
  so the seed, which applies only those resources, never had them.
- `HKCR\CLSID` itself opens for i386. Wine's server creates `Software\Classes\Wow6432Node`
  as a shared key (`server/registry.c`, `KEY_WOWSHARE`, set at load for every WoW64 machine).
  An i386 open of a first-level class key that `Wow6432Node` lacks falls back to the 64-bit
  `Software\Classes`, and the seed has no `Wow6432Node` at all, so i386 callers already
  read the 64-bit `CLSID`. In the same runs i386 `xaudio2_7.dll` and `mmdevapi.dll` load
  through `CoCreateInstance`, which reads `HKCR\CLSID\{…}\InprocServer32`. This is why
  the planned i386 registration into `Wow6432Node` was not done: it would not add the
  missing key, and a `Wow6432Node\CLSID` would end the fallback at the `CLSID` level and
  hide every 64-bit-only class from i386 callers. The supervisor chose to seed WIC's
  registration in the 64-bit classes only (decision 0072, option A of three: the other two
  were also mirroring every registration into `Wow6432Node`, and the original plan).

Confirmed by reading the source. No desktop Wine trace was run: the phone runs below show
the fix working.

## The change

`app/tools/prefix-registry.py` `register_wic` reads `regsvr.c`'s tables from the build's
patched Wine tree (`$PLAYPORT_RUN/pe/wine`) and the GUIDs and enum constants from its
generated `wincodec.h` and `wincodecsdk.h`. It writes what `register_categories`,
`register_decoders`, `register_encoders`, `register_converters`,
`register_metadatareaders`, `register_metadatawriters` and `register_pixelformats` write,
in that file's order (`DllRegisterServer`). The C tables are read by a small initialiser
parser: struct field names come from the struct definitions, entries stop at the loop's
terminator field, and pattern and mask bytes are cut to their length. A pattern that reads
past its table fails. `WIC_REGSVR_SHA256` pins the digest of those functions' source, so a
Wine move that changes them stops `pp registry` and the build's `--check`. Every key given a
value is marked for top-up.

Host tests in `tools/tests/test_prefix_registry.py`:
- the PNG decoder is an instance of the decoder category, with its pattern and mask;
- its existing class key is marked and carries `ContainerFormat`;
- the seed has no `Wow6432Node`;
- the parser, on a synthetic `regsvr.c` (GUIDs, octal and string byte arrays, struct
  arrays, terminators, a short entry);
- a pattern longer than its table fails;
- changed register code stops the generation.

## Registry diff

Reviewed as in [2026-10-08-proton-dll-parity.md](2026-10-08-proton-dll-parity.md): sections of
the committed `system.reg` before and after, compared with the Proton prefix's 64-bit view.

| | Count |
| --- | ---: |
| Sections before / after | 6,899 / 7,205 |
| Sections added | 306 |
| Existing sections given values | 37 (279 values: the WIC components' class keys, which their `WINE_REGISTRY` scripts create with `@` and `InprocServer32`) |
| Sections or values removed or changed | 0 |
| Top-up markers | 5 → 201 |
| New `InprocServer32`, `LocalServer32` or shell keys | 0 |
| Values equal to the Proton prefix's | 790 of 790 |
| Keys not in the Proton prefix | 2 (`PixelFormats\{…917}`, `{…91A}` under the default format converter: formats newer Wine converts) |
| Proton prefix WIC keys (class keys with `Author`="The Wine Project", their subkeys, the categories) not in the seed | 0 |

The change adds keys and values only, all under `Software\Classes\CLSID`. It adds nothing
that activates code: WIC creates its components inside windowscodecs. `user.reg` is
unchanged. `system.reg` grows from 1,088,851 to 1,151,405 bytes.

## Runs

One locked session: install the dev IPA in place, then the three plays. The container and
titles were kept, and the prefix was not reset.

| Run | IPA | JIT | First frame | Outcome |
| --- | --- | --- | --- | --- |
| `20261008T205411` | `c71c13d6` | 2.30 s | +3.70 s | Super Meat Boy to `first-frame+30`: alive, presenting, textures load; screen black |
| `20261008T205513` | `c71c13d6` | 2.46 s | +9.93 s | Hollow Knight to `first-frame+10`: main menu |
| `20261008T205603` | `c71c13d6` | 2.33 s | +5.59 s | Portal 2 to `first-frame+10`: the "powered by Source" splash |

- **Top-up of the existing prefix**, on the first launch after the install:
  ```
  [wine_host] registry system.reg: 306 of 7205 seed keys added, 279 values added to 37 present keys
  [wine_host] registry user.reg: 6 of 6 seed keys present, marked ones with their values
  ```
  This is exactly the diff above. The two later launches log `7205 of 7205 seed keys present`.
- **Super Meat Boy** (`--action play:epic-Buffalo --until first-frame+30 --shot`):
  - i386 `windowscodecs.dll` loads after "Creating Texture From File:
    resources/Fonts/EFIGS_0.png". No `NtOpenKeyEx` failure follows, and no
    "Unrecognized file format" appears in the run (50 in each run before).
  - d3dx9 logs `d3dx_load_pixels_from_pixels Unhandled filter 0x80004` 26 times. This is
    `D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER`, which Wine's d3dx9 applies as a point
    filter, as on the desktop.
  - The game goes on loading texture packages and audio cues. The `OutputDebugStringA`
    trace stops at the same point in every run, after 78 lines, so its end is not the
    game's.
  - DXVK 3.1.1 (Direct3D 9, i386, KosmicKrisp) creates two devices: a 640x480 swap chain,
    then 1564x720 FIFO. The presenter counts 2,240 frames by the end, with no DXVK error
    line.
  - The app ends the title at `--until`; there is no `crashes/`.
  - The screenshot is an entirely black screen.
- **Hollow Knight** (`--play app-367520`): the main menu (title, Start Game, Options,
  Achievements, Extras, Quit Game, version 1.5.12620).
- **Portal 2** (`--play app-620`): the "powered by Source" splash with Valve's copyright text.

## Next blocker (not fixed)

Super Meat Boy still shows black while it loads, decodes its textures and presents at
60 Hz. The log has no failing call left on this path, and there is no d3dx or DXVK error.
The candidates are:

- the i386 Direct3D 9 draw path: DXVK d3d9 through WoW64 winevulkan on KosmicKrisp;
- the game waiting on something before it draws its menu.

The next step is a desktop run per [HOST-DEBUGGING.md](../HOST-DEBUGGING.md): under Proton
with DXVK, to see what the game shows at +30 s, and an A/B with `DXVK_HUD` on the phone
(a dev page's Graphics options) to see whether DXVK's own overlay draws.

## Other run-time registrations wine.inf runs (not seeded)

`RegisterDllsSection` calls `DllRegisterServer` for these. Their `WINE_REGISTRY` parts are in
the seed (arm64ec set). What their code writes at run time is not, except where noted.

| Module | Run-time registration |
| --- | --- |
| `shell32` | `SHELL_RegisterShellFolders`: the shell folder keys (the seed's PROFILE covers the AppData ones) |
| `quartz` | DirectShow filter registration through its filter mapper |
| `colorcnv`, `msauddecmft`, `msmpeg2vdec`, `msvproc`, `resampledmo` | `MFTRegister`: in the seed (`MFTS`); their `DMORegister` (`HKCR\DirectShow\MediaObjects`) is not |
| `mp3dmod`, `msvdsp`, `vidreszr`, `wmadmod`, `wmvdecod` | `DMORegister` and `MFTRegister`: neither seeded |
| `mfh264enc` | `MFTRegister` (H.264 encoder): not seeded |
| `devenum`, `qcap`, `qedit`, `winegstreamer`, `l3codecx` | DirectShow filter and device category registration through the filter mapper |
| `cryptdlg`, `cryptnet`, `wintrust` | `CryptRegisterOIDFunction` / trust provider keys under `HKLM\Software\Microsoft\Cryptography` |
| `msisip` | the MSI subject interface package (`CryptSIP…`) |
| `mscoree`, `mshtml` | resources only on this path; Mono and Gecko installs (`control.exe`, disabled by 0071) |
| `urlmon` | `register_inf`: its INF-driven zone and protocol keys |
| `winevulkan` | the Vulkan ICD manifest in system32 and its `HKLM\Software\Khronos\Vulkan\Drivers` value |
| `wineps.drv`, `winprint` | the PostScript printer driver and print processor |
| `iexplore.exe` | its INF registration |
| `mfasfsrcsnk`, `mfmp4srcsnk` | resources only |

None was needed by a played title in this session. A title that needs one gets it by the same
reading of the module's source where it has tables (0072).

## IPAs

Built from this tree (`a5868be` plus this change), both verified (`pp verify`: dev 80
checks, release 84):

- dev `Playport-26.5-c71c13d6.ipa`, sha256
  `c71c13d6d06dc0d60a8cc06dfa3bb69aed21d535388aef397647eea83330939e` (on the phone; the runs
  above)
- release `Playport-26.5-release-d38af1cc.ipa`, sha256
  `d38af1cc1847ce41895576f03f90898c02c31ea7d940fb3c8e957e3e4a45a091` (not installed: the
  release app gets no automated phone tests)

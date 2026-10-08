# Plan: runtime DLL coverage (Proton parity)

**Linear:** [PLA-78](https://linear.app/playportdev/issue/PLA-78).

**Date:** 2026-10-07, rewritten 2026-10-08. **Kind:** execution plan.
**Status:** staging done on branch `dev/pla-78-proton-dll-parity`
([decision 0069](../decisions/0069-wine-modules-proton-parity.md),
[evidence](../evidence/2026-10-08-proton-dll-parity.md)); the rest is optional later work.

## Direction

Stop discovering missing Wine DLLs one failed play at a time. **Owner, 2026-10-08:
Proton parity, no census gate.** Stage every module Proton ships from Wine, that
is, each architecture's whole production build, keeping Playport's replacements.
This supersedes the 2026-10-07 choices that gated staging on a complete
owned-library PE census and put UI warnings in this plan; both are now optional
and listed under "Later, optional".

The policy is in decision 0069: the selection comes from
`build/generated/wine-pe-<arch>-windows.tsv` (no tests, no 16-bit modules, no
`winetest`, `control`, `winedbg` or `winemenubuilder`); arm64ec and i386 take the whole set,
aarch64 the WoW64 layer, every program and their import closure; FEX's
`xtajit64.dll`/`xtajit.dll`, the arm64ec `ntdll.dll` and DXMT's modules replace
Wine's of the same name; DXVK/vkd3d-proton (`Runtime/vulkan/`) and the Steam API
emulator (`Runtime/steamapi/`) stay in their own directories.

### Boundaries

- No component pin changes, upstream edits, downloaded system DLLs, automatic
  runtime repair, renderer change or per-title runtime packages.
- Staging adds no unix libraries or services: missing exports, spec stubs and
  unsupported behaviour (e.g. PLA-50's authentication packages, dxcore's missing
  D3D12Core exports) stay residuals with their own issues.
- No game binaries, credentials, signed URLs, account inventories or raw logs
  in git.

## Survey baseline (2026-10-07, research only)

The survey (`.work/agent-notes/dll-survey/report.md`, SHA256
`8052bd4f80ec3922bace05cec74ad6c5a0e0493dc4a9ae554624019574e52135`) read 161
storefront entries and 1,337 PE-named files. Before this change the built but
unstaged names were: i386 0 of 624, ARM64EC 453 of 597 (358.1 MiB raw), aarch64
486 of 597. Its sampled union of 16 names (`advpack`, `cabinet`, `iertutil`,
`shdocvw`, `glu32`, `twinapi.appcore`, `sensapi`, `dbgeng`, `dxcore`, `evr`,
`faultrep`, `msvcp90`, `msvcp_win`, `msvcr90`, `tbs`, `wer`) is now staged
in full. Its caveats still hold: a literal DLL string is a hint, not a call;
House of Golf 2's D3D12Core path asks for 91 exports the built dxcore lacks; and
a staged DLL is not proof that a feature works.

## Done: stage the production set (chunk 6 of the original plan)

`build/stages/stage-artifacts.py` (`production`, `selection`, `aarch64_closure`),
tests in `tools/tests/test_stage_selection.py`, regenerated `app/artifacts.tsv`
and `app/registry/`. Measured: +756 files, +479.1 MiB in a stored IPA; registry
seed 1,885 → 6,899 keys with no existing key changed. Phone checks (Hollow Knight,
Portal 2 to `first-frame+10`, ARM64EC and WoW64) and the before/after JIT pool
figures are in the evidence record.

## Later, optional

None of these gates the staging any more. Each needs its own Linear issue before
work starts.

1. **Owned-library census.** Bounded PE/managed-metadata parsing under
   `tools/compat/` with synthetic fixtures, an exact-build resolution of the
   runtime dependency graph, and a census of authorised Steam/Epic/GOG/phone
   bytes. Its use now is finding residual missing exports and spec stubs, not
   choosing DLLs. Storage access rules from the original plan stay: no new
   terminal store client, login or credential export; GOG needs the owner's
   sign-in through the UI.
2. **Exact-bundle catalogue.** A versioned, deterministic list of the staged
   providers' exports per variant, verified by `pp verify`.
3. **Install/adoption warnings.** A non-blocking "Runtime checks" section on a
   game's page from a bounded scan of the selected executable and its local DLLs,
   wording that never claims compatibility. Needs 1 and 2.
4. **Feature confirmations.** Plays that exercise paths the newly staged modules
   enable: Valheim's PlayFab/XCurl, Moonscars/Among Us GDK thunks, Snakebird's EOS
   credential persistence, an installer that runs `msiexec` or an aarch64-only
   program (`reg`, `taskkill`). A failure found here is filed per title in
   Linear, not fixed in passing.
5. **IPA compression.** The IPA is stored, not deflated; deflating the added
   modules would take them from 479.1 MiB to about 48 MiB in the download
   (installed size unchanged).

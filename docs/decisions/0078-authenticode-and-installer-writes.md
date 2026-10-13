# 0078: Authenticode as under Wine, and installer writes over the runtime's DLL links

**Status:** accepted, 2026-10-11, by the compatibility speedrun's worker with the owner away
(the conservative option at each choice, recorded for review);
[PLA-145](https://linear.app/playportdev/issue/PLA-145),
[PLA-144](https://linear.app/playportdev/issue/PLA-144),
[PLA-113](https://linear.app/playportdev/issue/PLA-113).
[Evidence](../evidence/2026-10-11-uepre-authenticode.md).

## Context

House of Golf 2's prerequisite installer (WiX Burn 3.8) checks its embedded payloads, the
VC++ 2015-22 redistributables and an MSI, with `WinVerifyTrust`, and on the phone every
check failed: `Failed to get signer chain from authenticode certificate`. A desktop A/B
under Proton 10 found why. wintrust's `DllRegisterServer`, which wineboot runs and the app
never does, writes the trust providers `WinVerifyTrust` loads its steps from, the
Authenticode ASN.1 decoders crypt32 lacks, and the subject interface packages. With no
trust provider, `WINTRUST_ACTION_GENERIC_VERIFY_V2` ran no step and returned success with
no signer. That is also a hole: every file "verified". The Microsoft roots were never the
question: Wine's crypt32 builds in Microsoft Root Authority and Root Certificate Authority
2010 and 2011 and adds them to the ROOT store, on the phone as under Proton.

Past verification, the redistributable's MSI writes its own `concrt140.dll`,
`msvcp140.dll` and `vcruntime140.dll` into `syswow64`. The app makes the prefix's
`system32`, `syswow64` and `sysarm64` links into the read-only bundle at each session start
(`wine_host.c` `link_system32`), so the write failed with access denied, and the package
with 1603. On a desktop prefix these are plain files.

## Decision

- **wintrust's and msisip's registration in the prefix seed**, as the rest of wineboot's
  registrations are (`app/tools/prefix-registry.py` `register_wintrust`,
  `register_msisip`): the OID encoders and decoders, the trust providers of every action
  wintrust registers, its default usages, its six SIPs, and msisip's Windows Installer SIP.
  The tables are ported from the build's Wine tree and guarded by a digest of the code they
  follow; the generated keys equal a desktop Proton prefix's for these two DLLs. The 64-bit
  view only, which i386 callers read too. cryptnet's revocation provider and cryptdlg's
  registrations are left out: the installer verifies without them on the desktop.
- **No certificate is shipped.** The roots Authenticode needs come from Wine's crypt32, as
  under desktop Wine and Proton; `Runtime/certs/cacert.pem` stays Mozilla's TLS roots.
- **A file created over a runtime link replaces the link** (wine-unix 0026). When
  `NtCreateFile` with `FILE_SUPERSEDE` or `FILE_OVERWRITE_IF` finds a symbolic link it
  cannot write through, it removes the link and creates the file in its place, as a desktop
  prefix's plain file would be overwritten. The next session start relinks every bundle
  DLL, so the runtime's own DLLs always come back; a file the bundle does not have (the
  redistributable's `mfc140u.dll`) stays. Opening a link for writing without truncation is
  unchanged.

## Why

Proton parity: the same installer, unchanged, verifies and installs under desktop Proton.
Registering what Wine registers is the smallest change and closes the "verifies anything"
hole. Shipping a Microsoft certificate would add a licensing question for nothing Wine
lacks. For the links, the alternatives were worse: copying the runtime into every prefix
costs hundreds of megabytes, and failing the write breaks every installer that carries a
CRT. Restoring the runtime's DLL at the next session keeps the runtime the one owner of the
DLLs it ships, which is how the app has always treated those directories.

## Consequences

- An installer that replaces a runtime DLL gets its file only until the session ends. A
  title that needs a native build of a DLL the runtime ships needs a DLL override instead,
  as under Proton.
- Signature checks now run: a payload with a broken or untrusted signature fails where it
  passed before, as on Windows.

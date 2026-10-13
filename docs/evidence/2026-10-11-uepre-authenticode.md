# Evidence: Burn's Authenticode check, VC++ files over runtime links, and a WoW64 window's exit

**Date:** 2026-10-11. **Linear:** PLA-145, PLA-144, PLA-113 (House of Golf 2's prerequisite
installer), PLA-67. **Decision:** [0078](../decisions/0078-authenticode-and-installer-writes.md).
**Status:** checked on the phone. Dev IPA sha256
`a81082393297571370a536b0ad8be1663e1b676983bd7eb7ec11ea0fb726e579` (the committed build):
the installer verifies its payloads, installs the VC++ 2015-22 x86 and x64 redistributables
and its MSI, exits `0x0`, and its process ends; `mfc140u.dll` is in the prefix's
`system32`. Hollow Knight (main menu) and Portal 2 ("powered by Source" splash) reached first
frame+10 on this IPA. House of Golf 2 itself was not replayed: it is no longer installed on
the phone (3.3 GB free).

The title: `UEPrereqSetup_x64.exe` (WiX Burn 3.8, i386, sha256 `3c6d8405…49f930e`), run as the
Local title `dir-uepre` with `/quiet /norestart`, after Wine Mono gave it .NET 4
([evidence](2026-10-11-wine-mono.md)).

## 1. No trust providers: WinVerifyTrust ran no step (PLA-145)

Before (run `$PLAYPORT_BUILD/agent-notes/pla125/run-uepre`, IPA of decision 0077):

```
e000: Error 0x80004005: Failed to get signer chain from authenticode certificate.
e000: Error 0x80004005: Failed to verify signature of payload: VisualStudioRedist_X86
i500: Shutting down, exit code: 0x4005
```

Burn calls `WinVerifyTrust(WINTRUST_ACTION_GENERIC_VERIFY_V2)`, then
`WTHelperProvDataFromStateData` and `WTHelperGetProvSignerFromChain(…, 0, …)`; the message is the
last one returning NULL. Wine's `WintrustLoadFunctionPointers` reads each step
(`SoftpubLoadMessage`, `SoftpubLoadSignature`, `WintrustCertificateTrust`, …) from
`HKLM\Software\Microsoft\Cryptography\Providers\Trust\<step>\{00AAC56B-…}` and leaves a step it
finds no key for empty. The seed had none of these keys (wintrust's `DllRegisterServer` writes
them, and the app runs no wineboot), so the action ran nothing, returned 0 and had no signer.

A/B under desktop Proton 10 in a scratch prefix (`$PLAYPORT_BUILD/agent-notes/pla145`), the same
installer, `/quiet /norestart /log`, one change per run:

| Run | Prefix | Result |
| --- | --- | --- |
| a | Proton's default | `i305: Verified acquired payload` ×3, exit `0x0`, `mfc140u.dll` in `system32` and `syswow64`; 7 s |
| b | a without `Cryptography\Providers\Trust` (both views) | the phone's failure exactly: `Failed to get signer chain…`, exit `0x4005` |
| c | a without `Cryptography\OID\EncodingType 1` | `0x80070002: Failed authenticode verification of payload` (no Authenticode decoders) |
| d | a without cryptnet's `CertDllVerifyRevocation` | verified, exit `0x0`: revocation is not needed |
| e | a with `Cryptography\OID` and `Providers` replaced by the seed's (trust providers and OIDs added, no msisip) | both VC++ payloads verified; `PrereqSetup` (an MSI) `0x800b0003` (TRUST_E_SUBJECT_FORM_UNKNOWN) |
| f | as e, the seed with msisip's SIP | all three verified, exit `0x0`, `mfc140u.dll` installed |

Every desktop run's process ended within seconds, so PLA-144 is phone-only.

**Fix (seed).** `prefix-registry.py` `register_wintrust` and `register_msisip`: the keys of
wintrust's and msisip's `DllRegisterServer`, 157 in all, equal key for key and value for value
to the desktop Proton prefix's for these two DLLs. The Microsoft roots were not missing: Wine's
crypt32 adds its built-in Root Certificate Authority 2010/2011 to the ROOT store on the phone as
on the desktop. On the phone (dev IPA `de4921985571ed62a2d9117c797ae3f13a27250e7b1446a360e2a4f8a998f1a5`,
the seed change alone, run `$PLAYPORT_BUILD/agent-notes/pla145/run-uepre1`):

```
i305: Verified acquired payload: VisualStudioRedist_X86 at path: C:\ProgramData\Package Cache\.unverified\VisualStudioRedist_X86, moving to: …\VC_redist.x86.exe.
i305: Verified acquired payload: VisualStudioRedist_X64 …
i305: Verified acquired payload: PrereqSetup …
i301: Applying execute package: VisualStudioRedist_X86, action: Install
```

## 2. The VC++ MSI could not write over syswow64's links

The same run: the x86 redistributable's `vcRuntimeMinimum_x86` MSI failed (`0x80070643`). Its
log stops after a rollback, and the host log names the write:

```
[file-wfail] ml669 #49 status=0xc0000022 disp=5 access=0xc0100080 … unix=…/prefix/dosdevices/c:/windows/syswow64/concrt140.dll
007c:err:msi:extract_cabinet FDICopy failed
007c:err:msi:ACTION_InstallFiles Failed to extract cabinet: L"cab1.cab"
```

`syswow64\concrt140.dll` is the session's link into the read-only bundle; on the desktop it is a
plain file. **Fix (wine-unix 0026):** a `FILE_SUPERSEDE` or `FILE_OVERWRITE_IF` create over a
link it cannot write through removes the link first (decision 0078).

## 3. The chain stopped when a child exited: a lock held by a killed thread (PLA-144)

Run 1 never ended (`no stop condition within --wait` 600 s). The census (`[thread-sample]`,
`[census-hold]`) at the hang: Burn's processes all waited in `wait_select_reply`, and the VC++
redistributable's clean-room process (`VC_redist.x86.exe`, tid `0068`) sat in
`__psynch_mutexwait`, holding the FEX lock, called from `ios_wow64_route_query`
(`NtQueryVirtualMemory`; offsets resolved against the IPA's `S1Probe`). Its child, the elevated
engine (tid `007c`), had exited just before:

```
[Wine ntdll/server] process_exit_wrapper(1603): closing child fd_socket=271
[wow64-window] release peb=0x128710000 base=0x74ffff0000 serviced_low_faults=4305 live_threads=0
[srv-kill] kill_thread tid=007c pid=0078 violent=0 rev=ml586
[fdtrace] VICTIM reply-read-eof tid=007c fd=272 ret=0 …
[section] thread abort: tid=007c leaves the section of mutex 0x1070f42f0 (fd_cache_mutex) it holds
```

The exit path closed the process's master socket, then released its WoW64 window; the window's
teardown made a server call, the server had already killed the thread, and the thread aborted
inside `ios_wow64_release_window` holding `ios_wow64_windows_lock` (no `torn down` line follows).
That lock is not an uninterrupted section, so the abort did not give it back, and the parent's
next `NtQueryVirtualMemory` waited for ever. Each earlier run's hang after Burn's exit code
(900 and 1500 s) is consistent with this. **Fix (madeira-unix 0110):** release the window before
the socket closes, where the fixed-base main image is already retired for the same reason.

## Result on the committed IPA

Run `$PLAYPORT_BUILD/agent-notes/pla145/run-uepre2` (IPA `a8108239…`), the Burn log
`UE_Prerequisites_(x64)_20261011021426.log`:

```
i319: Applied execute package: VisualStudioRedist_X86, result: 0x0, restart: None
i319: Applied execute package: VisualStudioRedist_X64, result: 0x0, restart: None
i319: Applied execute package: PrereqSetup, result: 0x0, restart: None
i399: Apply complete, result: 0x0, restart: None, ba requested restart:  No
i500: Shutting down, exit code: 0x0
```

The run's result: `done exit=0x00000000 after_s=6`. Of the eight WoW64 child exits, seven log
`[wow64-window] … torn down`; the eighth (Burn's elevated engine) is `retired` with one secondary
thread left, which the log does not show reclaimed before the session ended. No
`[section] thread abort`. The prefix's `C:\windows\system32` now has `mfc140u.dll`, `mfc140.dll`
and the MFC language DLLs (AFC listing). The payloads were already in the Package Cache from run
1, so this run logs no `i305` lines; run 1 shows the verification.

Still seen, not blocking: handled access violations at i386 eip `7bf9e4f0` (tid `0050`, PLA-144's
note), and `msi: __wine_msi_call_dll_function failed to load dll
C:\windows\temp\msiFE96.tmp (998)` for a custom action of `UEPrereqSetup_x64.msi`, which still
returned `0x0`.

Regressions on the same IPA: Hollow Knight (`app-367520`, run
`$PLAYPORT_BUILD/agent-notes/pla145/run-hk`): first frame at +8.65 s, the main menu at +10 s;
Portal 2 (`app-620`, `$PLAYPORT_BUILD/agent-notes/pla145/run-p2`): first frame at +5.84 s, the
"powered by Source" splash at +10 s.

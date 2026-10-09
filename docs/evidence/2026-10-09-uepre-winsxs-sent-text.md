# Evidence: the prefix's winsxs assemblies, and a WoW64 procedure's pointer for a sent message

**Date:** 2026-10-09. **Linear:** PLA-113 (House of Golf 2's prerequisite installer), PLA-67.
**Status:** checked on the phone with dev IPA sha256
`ddb449f65cdfdbdf2d8241191a5e2c38a264172bf950bb07d6f82c659b1a8fbc`: the installer gets past both
failures. It does not finish yet, for a third reason (below). Hollow Knight (`20261009T074652`,
main menu) and Portal 2 (`20261009T074748`, "powered by Source" splash) reached first frame+10
on this IPA.

The title: House of Golf 2's `UEPrereqSetup_x64.exe` (a WiX Burn 3.8 bootstrapper, i386), run
as the Local title `dir-uepre` with the arguments `/quiet /norestart`.

## 1. No `C:\windows\winsxs`: comctl32 5 instead of 6

With `env.WINEDEBUG=+class,+win` (run `20261009T071055`) the theme's last control failed:

```
0038:trace:win:WIN_CreateWindowEx L"SysLink" 00000000 -> not found
0038:err:wow:wow64_NtUserCreateWindowEx Playport: i386 NtUserCreateWindowEx( class "WixStdBA", ... ) failed, error 1400
0038:err:thread:RtlExitUserThread [thr-exit] ml806 RtlExitUserThread tid=0038 status=80070583 caller=7BEF42A8
```

`SysLink` exists only in comctl32 6. The bootstrapper's manifest asks for
`Microsoft.Windows.Common-Controls` 6.0, but the prefix had no `winsxs`, which wineboot fills
(setupapi's fakedll.c `register_manifest`, for every builtin with a `WINE_MANIFEST` resource) and
the app never runs:

```
002c:fixme:actctx:parse_depend_manifests Could not find dependent assembly L"Microsoft.Windows.Common-Controls" (6.0.0.0)
```

The activation context failed as a whole, so user32 loaded `system32\comctl32.dll` (version 5)
for `SysLink` and that has no such class. This affects every title whose manifest names
Common-Controls 6, or the VC80/VC90 runtimes, GdiPlus or MSXML, which wineboot installs the
same way.

**Fix (app).** The IPA carries the Wine pin's ten `WINE_MANIFEST` manifests as
`Runtime/winsxs/<module>/<file>.manifest` (provenance `winsxs`, under Wine's notice).
`wine_host.c` `link_winsxs` runs at each session start. For `x86` (the i386 set) and `arm64` (the
ARM64EC set, which ntdll takes for an `amd64` or `*` request in an x86-64 title), it writes
`winsxs/manifests/<arch>_<name>_<key>_<version>_none_deadbeef.manifest` with the architecture
filled in. It also creates `winsxs/<same>/` with links to the module's DLLs in the bundle. The
names match a desktop Proton prefix's, for example
`x86_microsoft.windows.common-controls_6595b64144ccf1df_6.0.2600.2982_none_deadbeef`, and the
manifest text is the same apart from the architecture. Wine's own (`_deadbeef`) manifests are
rewritten each time, because the bundle path changes on install. A title's own assemblies stay.

On the phone (`20261009T072015`): `winsxs: 20 assemblies installed, 0 failed`; `i386 image
C:\windows\winsxs\x86_microsoft.windows.common-controls_..._deadbeef\comctl32.dll`; the whole
theme was created and Burn started its elevated engine process.

## 2. A sent WM_SETTEXT reached the i386 procedure as a truncated address

Next (`20261009T072015`), the UI thread faulted when the engine thread set a theme control's
text:

```
[mach_exc] sym pc=S1Probe`set_window_text+0x8c lr=S1Probe`default_window_proc+0x510
[mach_exc] bt[3] ... call_window_proc+0x214  bt[4] ... peek_message+0x52c  bt[5] ... NtUserGetMessage+0x1b8
[exc-disp] raise tid=0038 code=c0000005 flags=0 addr=0x1028f052c pc=1028f052c nparams=2 p0=0 p1=707152d338
```

The process's guest window base was `0x7038950000`. The text was at guest `0x0028d338`, which is
host `0x7038bdd338`. win32u packs `WM_SETTEXT` only across processes, so the receiving thread got
the sender's window address. `wow64_NtUserCallWinProc` cut it to `0x38bdd338`. The procedure's
`DefWindowProcW` then converted that cut value through the window again, to `0x707152d338`, which
was never mapped.

**Fix (wine-pe 0035).** For an unpacked callback of a pointer message
(`is_guest_pointer_message`), a parameter inside the guest window converts back to its guest
address. Packed messages and values outside the window are unchanged. On the phone
(`20261009T072703`) the fault is gone, and Burn ran its apply phase.

## 3. Still open: Burn wants .NET Framework 4

The Burn log (`C:\windows\temp\UE_Prerequisites_(x64)_20261009052717.log`):

```
i000: Registry key not found. Key = 'SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full'
w343: Prompt for source of package: NetFx40Redist, payload: NetFx40Redist, path: C:\Games\uepre\redist\dotNetFx40_Full_x86_x64.exe
i338: Acquiring package: NetFx40Redist, payload: NetFx40Redist, download from: http://go.microsoft.com/fwlink/?LinkId=247729
e000: Error 0x80004005: Failed to get signer chain from authenticode certificate.
i500: Shutting down, exit code: 0x2
```

Desktop Proton has Wine Mono, which registers `NDP\v4\Full`, so its Burn log has
`NETFRAMEWORK40 = 1` and `NetFx40Redist, state: Present`, and it installs only the VC++ 2015-22
runtimes and its own MSI. The phone's prefix has no .NET, so Burn plans the .NET 4 redistributable,
which is not in the bundle, and the download fails. MFC is not installed.

### Experiment: with .NET 4 "present"

As a one-off test (nothing committed), Wine's i386 `regedit.exe /s` wrote
`NDP\v4\Full` (`Install`=1, `Version`=`4.8.03761`, `Release`) and `NDP\v4\Client` into the
phone's prefix, from a scratch Local title (`dir-netfx`, run `20261009T081018`). The installer then
ran again (`20261009T081101`, log `UE_Prerequisites_(x64)_20261009061119.log`). Passing
`NETFRAMEWORK40=1` on the command line instead does not work: Burn's registry search clears it.

```
i101: Detected package: NetFx40Redist, state: Present, cached: Partial
e000: Error 0x80004005: Failed to get signer chain from authenticode certificate.
e000: Error 0x80004005: Failed to verify signature of payload: VisualStudioRedist_X86
e000: Error 0x80004005: Failed while caching, aborting execution.
i500: Shutting down, exit code: 0x4005
```

So .NET detection is not the last blocker. The next one is Burn's Authenticode check of its
embedded payloads (the VC++ redistributable): wintrust/crypt32 returned no signer chain on the
phone, but desktop Proton verifies the same payloads (`i305: Verified acquired payload`). After
Burn logged its exit code, the title process also did not end within the run's wait (900 to
1500 s). The keys were then removed with `regedit /s` and a `[-HKLM\...\NDP\v4]` file (run
`20261009T083959`). An empty `NET Framework Setup\NDP` key remains, with no values or subkeys.

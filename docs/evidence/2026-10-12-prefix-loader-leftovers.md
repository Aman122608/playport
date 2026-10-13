# 2026-10-12: prefix and loader leftovers: machine values, xaudio2_9redist.dll, win32u's thread info key, CurrentControlSet

**Linear:** [PLA-168](https://linear.app/playportdev/issue/PLA-168) (wineboot's machine values),
[PLA-167](https://linear.app/playportdev/issue/PLA-167) (`xaudio2_9redist.dll`),
[PLA-157](https://linear.app/playportdev/issue/PLA-157) (win32u unix init per process),
[PLA-166](https://linear.app/playportdev/issue/PLA-166) (typelibs, `CurrentControlSet`). Decision
[0098](../decisions/0098-machine-values-at-session-start.md). The owner was away; the conservative
option was taken at each choice and is recorded there.

**IPA:** `Playport-26.5-2bec1330.ipa` (dev), sha256
`2bec1330d8614ce87651f954093e5e4920fa5d98dcfe2f08a46117c2132ae455`, built from commit 74f1212 and
installed in place. Before it, the phone had `0521958c…` (86d91c1). Run directories are under
`$PLAYPORT_BUILD/agent-notes/sweep10a/` and `$PLAYPORT_BUILD/perf/sweep10a-after-1`.

## The probe

`tools/compat/probes/machine.c`, built by `tools/compat/probes/build.sh` for x86-64 (`machine.exe`)
and i386 (`machine-i386.exe`). Each folder also holds `xaudio2redist.exe`, a program whose only
import beyond kernel32 is `xaudio2_9redist.dll`'s `XAudio2Create`, in the folder itself and again
in `redist/`. For the x86-64 runs a title's own copy of `xaudio2_9redist.dll` (from an Unreal
title's `Engine/Binaries/ThirdParty/Windows/XAudio2_9/x64`, not committed) was put in `redist/` by
hand. The folders were put in `Documents/Import` over AFC and imported through the UI
(`dir-machine 2`, `dir-machine-i386 2`). The same files ran under desktop Proton Experimental
(11.0) in a fresh scratch prefix.

## PLA-168: what a guest saw, and what it sees now

| Value (x86-64 process) | Proton Experimental, x86-64 desktop | Phone before (`0521958c…`) | Phone after (`2bec1330…`) |
| --- | --- | --- | --- |
| `GetSystemInfo` | AMD64, level 26, revision 4400, 16 processors | AMD64, level 6, revision a601, 6 processors | same |
| CPUID | AuthenticAMD, family 26 model 68 | GenuineIntel, family 6 model 166 stepping 1; brand string garbage (below) | same |
| env `NUMBER_OF_PROCESSORS` | 16 | absent | 6 |
| env `PROCESSOR_ARCHITECTURE` | AMD64 | AMD64 (ntdll sets it) | AMD64 |
| env `PROCESSOR_IDENTIFIER`, `_LEVEL`, `_REVISION` | `AMD64 Family 26 Model 68 Stepping 0, AuthenticAMD`, 26, 4400 | absent | `Intel64 Family 6 Model 166 Stepping 1, GenuineIntel`, 6, a601 |
| `COMPUTERNAME`, `GetComputerNameW` | the host's name | PLAYPORT | PLAYPORT |
| env `USERDOMAIN`, `LOGONSERVER` | the host's name, `\\` and it | absent | PLAYPORT, `\\PLAYPORT` |
| `CLIENTNAME`, `SESSIONNAME`, `HOMESHARE` (Volatile Environment) | Console, Console, empty | absent | Console, Console, empty |
| `HARDWARE\DESCRIPTION\System` | `AT compatible`, `01/01/70`; 16 `CentralProcessor` and `FloatingPointProcessor` keys | no key | `AT compatible`, `01/01/70`; 6 and 6 keys |
| `CentralProcessor\0` | `AMD64 Family 26 Model 68 Stepping 0`, AuthenticAMD, `AMD Ryzen 7 9700X 8-Core Processor`, ~MHz 3793, FeatureSet 30b93dfe | no key | `Intel64 Family 6 Model 166 Stepping 1`, GenuineIntel, `Apple Silicon`, ~MHz 1000, FeatureSet 2379ffff |

An i386 process gets the same values. Its `PROCESSOR_ARCHITECTURE` is `x86` on both. Its
`PROCESSOR_ARCHITEW6432` is `AMD64` under Proton and `ARM64` on the phone, where ntdll gives the
native machine and `IsWow64Process2` says `aa64`. That was left as it is (decision 0098).

**Where the values come from.** The probe also read wineboot's inputs as an x86-64 process sees them
on the phone:

- `SystemCpuInformation`: level 6, revision a601, features 2379ffff;
- `ProcessorInformation`: 1000 MHz;
- RDTSC: 1000.0 MHz against `QueryPerformanceCounter` (under Proton RDTSC gave 3792.9 MHz, which
  is wineboot's ~MHz there: 3793);
- SMBIOS processor: vendor "Apple", version "" (no name), 6 threads.

The host writes these values, except that the vendor is CPUID's and the name is FEX's "Apple
Silicon" (decision 0098).

**The write.**

- The first launch on the new IPA logged `registry machine values (6 processors): 13 of 14 seed
  keys added, 5 values added to 1 present keys`.
- Every later launch logged `14 of 14 seed keys present`, so nothing was written again.

**Probe results.**

- x86-64 (`fix-x64`): `machine: done ok`, every CHECK `ok`.
- i386 (`fix-i386`): `done ok`.
- Before, on `0521958c…` (`base3-x64`, `base2-i386`): `done FAIL`. The environment and registry
  CHECKs failed.

Host test: `app/tests/prefix_registry_test.c`. A stale count is rewritten after the old line, and
other values stay untouched. There is one key a processor. A second run writes nothing.

**Found on the way: FEX's CPUID brand string is garbage on the phone.** Leaves 0x80000002-4 read
`PerCPUData[CPU].ProductName` for the CPU the thread runs on. On iOS, FEX's host features are
sized to one processor (`patches/fex-port` 0009), so on any core but 0 the read goes past the end
of the per-CPU array. The x86-64 run read `"H�0"`, then `"H���v!"`; the i386 run read
`"��E�"`. That is an out-of-bounds read in the emulator, and a follow-up. It is not changed here.

## PLA-167: `xaudio2_9redist.dll`

- **Neither Wine nor Proton provides it.**
  - The pin's Wine builds `xaudio2_0` to `xaudio2_9` and no `xaudio2_9redist`.
  - Proton 10.0, 11.0 and Experimental ship only `xaudio2_9.dll` (x86_64 and i386).
  - A fresh Proton prefix has no `xaudio2_9redist.dll` either.
- **Titles that need it ship it.** Unreal titles keep it in
  `Engine/Binaries/ThirdParty/Windows/XAudio2_9/x64`, as three Epic manifests on the workstation
  list. The EOS SDK documents that a title ships it beside the SDK.
- **The phone matches Proton:**

  | | Proton Experimental | Phone (both IPAs) |
  | --- | --- | --- |
  | `LoadLibrary("xaudio2_9redist.dll")` by the search path | fails, error 126 | fails, error 126 |
  | a program importing it, no DLL beside it | ends `c0000135` | ends `c0000135` |
  | a title's copy: `LoadLibrary` by path, `XAudio2Create` | loads, `S_OK` | loads, `S_OK` |
  | a program importing it from beside a title's copy | exit 0 | exit 0 |

- **Nothing was added.** The owner's rule is met: a title that works under Proton finds the same
  DLLs here.
- **Sniper Elite 5.** Its copy on the workstation is gone (308 KB left), so whether its EOS SDK
  imports the DLL statically or delay-loads it could not be checked again.
- **Next step:** only if a title fails on the phone with this DLL missing, look at where its own
  copy is.

The probe's import child must run before the parent makes an XAudio2 engine. Under desktop Proton, a
child started after that did not return from `XAudio2Create` within 60 s. Started by `cmd`, the same
child ran in 0.8 s. This is a desktop-only quirk, not investigated.

## PLA-157: win32u's unix init per guest process

- On iOS, every guest process of a session shares win32u's unix library, and
  `__wine_unix_lib_init` runs at each process's win32u load. What it does:
  - `KeAddSystemServiceTable(…, 1)` sets the same table again. It is harmless.
  - `ntdll_add_syscall_debug_info` sets the same names again. It is harmless.
  - `wow64_zero_bits` gets the same mask from every i386 process. wine-unix 0027 already moved
    `zero_bits` to the calling thread.
  - `pthread_key_create(&user_thread_info_key)` makes a **new key over the old one**. Every thread
    of a process already running then finds no `user_thread_info` under the new key. It gets a
    fresh, empty one: no queue handle, desktop or message window, hook state, message being
    received or OpenGL thread data. The old one leaks.
  - This happens whenever a second process loads win32u: a launcher and its game, a game's child
    (dxdiag, a crash reporter), a prerequisite installer. It also uses one of the host's 512
    pthread keys each time.
- **Fix:** `patches/wine-unix` 0032 makes the key once (`pthread_once`) under `WINE_IOS`. The rest
  of the init is left as it is.
- **Phone:** no probe makes the lost state visible from user mode; the fields are recomputed or
  transient. The fix was checked by the gates and the probe's children (`xaudio2redist.exe`
  started from the probe, both IPAs).

## PLA-166: typelibs and `CurrentControlSet`

- **`CurrentControlSet` is a plain key on the phone; desktop has a link.**
  - The probe on the phone: `CurrentControlSet is a plain key`. Desktop Proton: `a link
    (SymbolicLinkValue)`, with `Select\Current` = 1 on both.
  - wineserver makes the link only when `system.reg` does not exist yet
    (`server/registry.c init_registry`). The app writes the seeded hive before the server first
    loads it, so the link is never made.
- **Who writes `ControlSet001`, and who reads it.**
  - The only writer of `ControlSet001` in the shipped code is DXMT's dxgi
    (`InitializeVendorExtensionNV`). It creates `SYSTEM\ControlSet001\Services\nvlddmkm` and, with
    `DXMT_ENABLE_NVEXT` unset (Playport never sets it), deletes the NVIDIA value again. So the key
    is empty: the phone's `ControlSet001` has that one subkey.
  - Nothing writes `ControlSet001` for a reader of `CurrentControlSet`. The seed and every Wine
    writer use `CurrentControlSet`.
- **Typelibs.** `LoadRegTypeLib(stdole2 2.0)` and `LoadTypeLib("stdole2.tlb")` both return `S_OK`
  on the phone, as under Proton. The seed carries no `TypeLib\{00020430-…}` key; Wine's desktop
  prefix has 7. oleaut32 finds the library another way (`query_typelib_path` also asks the
  activation context).
- **No change. Next step:** only if a title reads a value through `CurrentControlSet` that something
  wrote through `ControlSet001`, or fails a typelib lookup. The link would need a migration of
  existing prefixes (decision 0080).

## Gates and perf (IPA `2bec1330…`)

| Run | What | Result |
| --- | --- | --- |
| `gate-hk` | Hollow Knight `app-367520 --until first-frame+10 --shot` | first frame +10.35 s; main menu |
| `gate-p2` | Portal 2 `app-620 --until first-frame+10 --shot` | first frame +6.11 s; Source splash |
| `perf/sweep10a-after-1` | Portal 2 `pp perf --secs 120`, The Cold Boot | below |

`pp perf --compare`, window 75-120 s:

| Run | IPA | FPS | GPU ms | M instructions a frame |
| --- | --- | --- | --- | --- |
| `perf/pla186-after-1` | `0521958c…` | 60.0 | 15.2 | 60.1 |
| `perf/sweep10a-after-1` | `2bec1330…` | 60.0 | 15.2 | 60.2 |

There was no measurable cost.

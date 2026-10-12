# 0098: wineboot's machine values are written at every session start

**Status:** accepted, 2026-10-12, by the sweep 2.10a worker with the owner away (the conservative
option at each choice, recorded for review); [PLA-168](https://linear.app/playportdev/issue/PLA-168).
Amends [0080](0080-prefix-wineboot-parity.md), which left these values as a follow-up.
[Evidence](../evidence/2026-10-12-prefix-loader-leftovers.md).

## Context

wineboot writes some values from the machine at every start, which a static seed cannot carry:
`create_hardware_registry_keys` writes `NUMBER_OF_PROCESSORS` and `PROCESSOR_ARCHITECTURE`,
`_IDENTIFIER`, `_LEVEL` and `_REVISION` into `Session Manager\Environment`, and
`HARDWARE\DESCRIPTION\System` with a `CentralProcessor` and `FloatingPointProcessor` key for each
processor; `create_volatile_environment_registry_key` writes `USERDOMAIN`, `LOGONSERVER`,
`CLIENTNAME`, `SESSIONNAME` and `HOMESHARE`. The app runs no wineboot. On the phone a guest had none
of them: no `NUMBER_OF_PROCESSORS`, `PROCESSOR_IDENTIFIER`, `_LEVEL`, `_REVISION` in its
environment, no `HARDWARE\DESCRIPTION\System` key. ntdll already gives each process
`PROCESSOR_ARCHITECTURE`, and the seed already names the computer (`PLAYPORT`, decision 0080's
computer name keys).

## Decision

- **The host writes the processor values before the wineserver loads `system.reg`** (`wine_host.c`
  `seed_machine_values`, text from `prefix_registry_machine_values`). The first guest process's
  environment is built from the registry before any guest code runs, so they cannot come from a
  guest process. A value already right is not written again; a changed one is appended after the
  old one, which the server replaces as it loads (`prefix_registry_set`).
- **The values are what wineboot's code computes from what an x86-64 guest sees on the phone**,
  as the machine probe reads them: the processor count is the phone's (`sysconf`, as ntdll counts
  them), level 6 and revision `a601` (FEX's CPUID signature), the feature bits `2379ffff`, and
  `~MHz` 1000 (the guest's RDTSC rate, which is what wineboot's x86 `read_tsc_frequency` measures).
- **Two inputs differ from wineboot's on iOS, and the x86-64 machine's answer is taken.** The SMBIOS
  processor says vendor "Apple" and no name. `VendorIdentifier` and `PROCESSOR_IDENTIFIER`'s vendor
  are `GenuineIntel`, the guest's CPUID vendor, as on an x86-64 machine where SMBIOS and CPUID agree.
  `ProcessorNameString` is "Apple Silicon", FEX's own name for an Apple core, rather than absent
  (Proton always has one).
- **The Volatile Environment values are in the seed** (static: the computer name is fixed):
  `USERDOMAIN=PLAYPORT`, `LOGONSERVER=\\PLAYPORT`, `CLIENTNAME` and `SESSIONNAME` `Console`, an
  empty `HOMESHARE`, topped up as the other profile values are.
- **Left as they are:** the `BIOS` subkey (from iOS's SMBIOS, which carries the phone's serial
  number), `PROCESSOR_ARCHITEW6432` of an i386 process (ntdll gives `ARM64`, the native machine,
  where Proton on an x86-64 machine gives `AMD64`; changing it would disagree with
  `IsWow64Process2`), and the keys being volatile (they are saved in `system.reg`, so a processor
  key above a smaller count stays).

## Why

The issue's direction: write the missing values at session start, matching what Proton shows for
an x86-64 machine, with the phone's processor count. The guest's own `GetSystemInfo` and CPUID
already describe that machine; the registry and environment now say the same. A game that reads the
processor name or count from the registry or environment (engines' system reports, job systems)
finds them, as under Proton.

## Consequences

- `system.reg` gains `Hardware\Description\System` (two values), 2 keys a processor (12 on a
  six-core phone) and five `Session Manager\Environment` values at the first launch; later launches
  write nothing unless a value changed.
- The level, revision, feature bits and MHz are constants in `prefix_registry.c`, from the probe on
  the FEX pin: a FEX or ntdll change to what the guest sees needs them updated. The probe's CHECK
  lines (`tools/compat/probes/machine.c`) compare the environment with `GetSystemInfo` on the phone.

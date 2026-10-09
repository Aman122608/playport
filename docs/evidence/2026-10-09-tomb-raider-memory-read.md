# Tomb Raider: faulting self-memory read

**Linear:** [PLA-150](https://linear.app/playportdev/issue/PLA-150/tomb-raider-203160)

## Observed failure

The owner's Steam Tomb Raider (203160), i386 `TombRaider.exe`, failed before
first frame on three launches in the log fetched on 2026-10-09:
`.work/agent-notes/tomb-raider/initial/s1-host.log` (previous log preserved in
`.work/agent-notes/tomb-raider/previous/`). Installed dev IPA sha256:
`001194008db3208b2950f441b899e08cac6a031e9cbfa615e3bd66c728dfbe9f` (HEAD `d0a5eb0`).

The last launch ended 361 ms after game start with `0xc0000005`. After creating
its crashpad directory, the guest called `NtReadVirtualMemory` on an unreadable
page. Native `_platform_memmove+0x90`, called from `NtReadVirtualMemory+0xf0`,
tried to read 0x7c8 bytes from a PROT_NONE reservation just before the WoW64 TEB.
Wine's exception dispatcher rejected the native syscall-stack frame as outside
its stack limits and terminated the pseudo-process. The `__TRY` around the
copy therefore did not turn the bad read into `STATUS_PARTIAL_COPY`.

This is not evidence of the Epic edition's earlier opengl32 failure (PLA-139).
No desktop game reproduction was needed to locate this iOS-native fault.

## Change and verification

`patches/madeira-unix/0106` replaces the current-process `memmove`/exception
handler with `mach_vm_read_overwrite`. Inaccessible or concurrently unmapped
source memory returns an error without faulting on the native syscall stack.
Zero-length reads still succeed; failed/short reads report `STATUS_PARTIAL_COPY`
and zero bytes. The existing destination probe and remote-process path are
unchanged; this is not a general fix for syscall-stack exception dispatch.

`tools/tests/test_ios_read_memory.py` compiles the exact added adapter with a
mock Mach API, covering full and unaligned reads, zero length, failure and
short results. This does not validate Darwin kernel behaviour.

`./pp test --quick` passed, including the new adapter test. `./pp build`
passed all 80 IPA checks; installed in place, container kept. Candidate dev IPA:
`aaa1afd0b0d650210ac219c5775e9914b3b26a6f1a6e313f88e0913b0336b0c2`.

- Tomb Raider: `.work/agent-notes/tomb-raider/mach-read`. The native read fault
  is gone, but a later guest write faults at `TombRaider.exe` VA `0x6e874b`
  (`mov [eax+4],edi`, destination `0x1052211c`). Immediately before that,
  `VirtualFree(MEM_DECOMMIT)` removed the whole 16 KiB host page containing
  live heap metadata before the requested 4 KiB-aligned range. Its crash
  handler exits with code zero: the driver's `ok` is **not** a boot pass.
  No first frame; title remains Broken. Further work stays in PLA-150.
- Hollow Knight: `.work/agent-notes/tomb-raider/hk-regression`, first frame
  +9.34 s, `first-frame+10` passed, screenshot shows the main menu and touch
  controls. Pool not exhausted; band refused and runtime-limit counters zero.

These are launch checks, not gameplay/performance qualification.

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

## WoW64 heap trim

`patches/madeira-unix/0107` makes `MEM_DECOMMIT` round to 4 KiB Wine pages,
not 16 KiB host pages. Only fully covered host pages are remapped. Shared
boundary pages keep their backing and neighbouring contents; the requested
bytes are zeroed and host protection is recomputed from the surviving logical
page permissions. Both edges are prepared before destructive operations;
preparation/remap failures restore protections without changing contents or
caller outputs. Release of an entire allocation is unchanged.

`build/wow64/vm_test.c` exercises the actual patched header with real mappings
and simulated 16 KiB host pages. It covers a heap trim with two partial edges
and whole interior pages, preservation of read-only neighbours, zero-on-recommit,
first/second-edge protection failures, mmap failure, single-page edges, repeated
decommit, and leading-only/trailing-only/full-page spans. As with the existing
WoW64 protection policy, inaccessible 4 KiB pages sharing a host page with an
accessible neighbour are enforced logically, not by sub-host-page hardware.

The first test compile caught a duplicate test-local variable name; after its
correction `./pp test --quick` passed. The dev build passed all 80 IPA checks:
`e530fe724bcfe9c52dc03807b412d9edae42de6853f78aa94c40fa0bb11c954e`.
Installed in place. Tomb Raider run `.work/agent-notes/tomb-raider/decommit`
passes the heap trim and reaches D3D9 adapter enumeration, then crashes during
its D3D11 probe: a NULL function call in native `init_physical_device+0x34a8`.
The object disassembly identifies the placed-memory Properties2 query (core
Vulkan 1.1) reached through D3DKMT's Vulkan 1.0 instance with the KHR extension.
The preceding Features2 query uses the KHR entry point successfully. This is a
separate graphics-initialization bug, not a first-frame pass.

Hollow Knight `.work/agent-notes/tomb-raider/hk-decommit` passes
`first-frame+10` (first frame +10.24 s); screenshot shows its main menu with
touch controls. No pool exhaustion, band refusal or runtime-limit counters.

## D3DKMT placed-memory query

`patches/wine-unix/0025` makes win32u's `init_physical_device` query
`VkPhysicalDeviceMapMemoryPlacedPropertiesEXT` through
`vkGetPhysicalDeviceProperties2KHR`, the entry point D3DKMT's Vulkan 1.0
instance enables, as the Features2 query before it already does. Dev IPA
`348a67da8192a776b54b91b21721da5835a94421616a49edddaeac7dbbc179d3`,
installed in place.

- Tomb Raider `.work/agent-notes/tomb-raider/vulkan-query`: the NULL call is
  gone (the log reports the i386 placed alignment, 16384); the game stays up
  for the 35 s window. Without `-nolauncher` there is no first frame: its
  launcher is a GDI window the phone does not draw.
- With `-nolauncher` (`nolauncher`, `menu-check`, and `aspect169` at 16:9)
  the game presents at about 60 FPS from +4.2 s, but every screenshot is black
  apart from two short, pulsing horizontal segments at mid-height (the same
  game-relative place at 16:9), which matches the owner's report of a thin
  flashing line. Its D3D11 renderer (`PCDX11*`; the executable has no D3D9
  renderer) runs on Wine's i386 `d3d11`/`dxgi` over wined3d, which finds no GL
  (`wined3d_adapter_gl_init` fails): the Vulkan overlay ships only DXVK's i386
  `d3d9.dll` (`build/stages/vulkan-pe.sh` `stage_dxvk32`). Title stays Broken.
- Hollow Knight `hk-vulkan-query`: first frame +9.69 s, `first-frame+10`
  passed, screenshot shows the main menu.

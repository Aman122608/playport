# Jurassic World Evolution: a hung commit probe and unaligned atomics (PLA-41)

**Date:** 2026-10-09. Phone: iPhone18,4, dev build. Title: Jurassic World Evolution
(`epic-373a4372c20540aca7fdd880d27fa49a`, build 1.12.7.54973), Vulkan, 16:9. Run directories are
under `$PLAYPORT_BUILD/ui-runs/`.

## Before (IPA `1335554d…`, run `20261009T031646`)

The executable window (2026-10-07) holds: `JWE.exe` maps at 0x140000000. The DRM passes and
decrypts the image page by page (about 75 000 `NtProtectVirtualMemory` calls, 0x80 → 0x20, all
within 5 s). At +5.26 s the main thread allocates a 1 MB thread stack committed to its full
reserve, and the process stops: no more server requests (`[srv] … req=0`) and no frame for the
rest of the run.

    mprotect_exec(0x7030370000, 0x4000, ---)
    mprotect_exec(0x7030374000, 0xfc000, rw-)
    [store-noalias] #5 rev=ml348 addr=0x7030372000 insn=0x3869680a pc=0x1052d9210 NO pool/anon alias | region 0x7030370000+0x4000 prot=0 max=7
    [mach-deliver] rev=ml378 no TEB owns sp=0x702e6af8f0 (pc=0x1052d9210 …) -> BEST-EFFORT delivery on guest stack
    [thread-sample] ml876 b0 "Main" … pc=S1Probe`ios_verify_commit_zero+0xc8 lr=S1Probe`allocate_virtual_memory+0x654

The stack's commit starts 8 KB above its no-access and guard pages, in their 16 KB host page.
A host page's protection is the union of its four Windows pages', and a guard page makes it
`PROT_NONE`. `ios_verify_commit_zero`, a diagnostic that reads the first bytes of each fresh
commit, read that page natively with `virtual_mutex` held, faulted, and the fault could not be
delivered.

## Fix 1 (madeira-unix 0102)

The probe reads through `mach_vm_read_overwrite` and does not check a page the host cannot read.

IPA `836dee6c94559b39147332f5055adccde79af64b69788b8664936ce5234d3739`, run `20261009T033159`:
the hang is gone. The game makes 19 threads and ends `0xc0000005` 7 s in. Its DRM runs `xchg`
on unaligned stack slots. FEX completes each one on the SIGBUS path (`Handled unaligned
atomic`), but the Mach exception server counts each pass twice and dispatches an access
violation on the third identical (thread, pc, address) fault. A loop that ran the same atomic
on the same address a second time got `c0000005`:

    [unaligned-guest] REFUSED-OTHER insn=0xf8fb80db addr=0x702e01dcff pc=0x11deda264 ... keeping 80000002
    D 38 Handled unaligned atomic: new pc: 11DEDA268
    [mach-deliver] rev=ml369 #13 code=c0000005 pc=0x11deda264 excaddr=0x11deda264 addr=0x702e01dcff ...
    0038:err:seh:NtRaiseException Unhandled exception code c0000005 flags 0 addr 0x154eec10a

## Fix 2 (madeira-unix 0103)

A data abort with the alignment-fault status (DFSC 0x21) always goes to the SIGBUS path. It is
never an x86 access violation.

IPA `d638478a48d87a887edf28bc3471b0e4e179295a5c7345ab9c9f2a75d0a80ea8`, run `20261009T034102`:
the unaligned atomics on the same addresses now complete again and again. The game still ends
`0xc0000005` 7 s in, in DRM code, with no frame. The fault is a byte store into the game's own
4 GB `MEM_RESERVE` (made at `JWE.exe+0x98967e`, base `0x7038950000`), at an offset that was
never committed:

    [fault-rgn]   IN WINE VIEW base=0x7038950000 size=0x100000000 protect=0x3 off=0x8f856ba9 end=0x7138950000
    D 34 Exception: Code: C0000005 Address: 1488A3CF6 AV WRITE of 70C81A6BA9
    0034:err:seh:NtRaiseException Unhandled exception code c0000005 flags 0 addr 0x1488a3cf6

The instruction at `0x1488a3cf6` is `mov [r11-0x62d98ac8], r8b`, with `r11=0x712af3f671`.
With FEX's full SMC checks (`runtime` `env.FEX_SMCCHECKS=full`, run `20261009T034845`) the game
runs 51 s, slowly, and dies at the same RIP with another address in the same reserve
(`AV WRITE of 710120D688`). So the cause is not a stale translation of the decrypted code. The
pointer is computed, not stored, and no vectored handler is involved. The cause is open: it may
be a wrong value from the emulation (for example registers or flags lost on the
unaligned-atomic exception round trip), or a commit the runtime lost.

# Mortal Kombat Komplete Edition: forced PROT_EXEC broke an i386 start

**Date:** 2026-10-09. **Title:** Mortal Kombat Komplete Edition (Steam 237110), imported as a
Local title from a Windows install folder. **Phone:** iPhone18,4, iOS 27.0, dev builds,
Vulkan, 16:9.

## The failure

IPA `dbd94bf4…` (`dbd94bf4781126fdfdd8e742c747f6ebb7f256cd34e2905c2d053436f4eb6d82`):
`DiscContentPC\MKKE.exe` (i386, Unreal Engine 3, Direct3D 9) ended about 10 s after the
JIT, with no window. In its WoW64 start, the import table of `wow64.dll` was made writable,
bound, and restored to read-only; the restore failed, and the first import call jumped to
an import's RVA:

```
NtProtectVirtualMemory(orig=0x73fdaf3000 reversed=0x73fdaf3000 sz=0x1000 prot=0x2)
[vmem-denied] set_vprot failed: base=0x73fdaf3000 size=0x1000 protect=0x2
[mach_exc] UNHANDLED #1 pc=0x34f4c addr=0x34f4c x18=0x0 type=1 lr=0x106d4d0f4 ...
[mach_exc] lr PE: wow64.dll+0x1d0f4 (va=0x73fdadd0f4)
[redeliv] terminating process rev=ml465
```

Portal 2 (also i386) takes the same step and the restore succeeds. The difference is the
executable's `DllCharacteristics`: `MKKE.exe` has `0x8000`, without
`IMAGE_DLLCHARACTERISTICS_NX_COMPAT` (`portal2.exe` and `MKLauncher.exe` have it). For such
a main executable Wine's loader sets `ProcessExecuteFlags` to `MEM_EXECUTE_OPTION_ENABLE`,
and `virtual_set_force_exec` makes `mprotect_exec` ask for `PROT_EXEC` with every
`PROT_READ`. iOS never grants that outside the JIT pool, so every read-only request failed
(`mprotect_exec` returns -1 before it logs), and the import table's pool copy was never
synced.

## The fix

`patches/madeira-unix` 0104: on iOS, `ProcessExecuteFlags` is recorded but no longer
forces host `PROT_EXEC`. Guest code runs through FEX, which keeps EXEC in Wine's logical
protection, and `force_exec_prot` is one flag for every pseudo-process in the one Mach
process.

## After the fix: a hang when its dxdiag child exits

IPA `994b287e…` (`994b287e0c86e97dee069b46200986c6f5d03949b16df9686e57411b933843c1`):
the WoW64 start completes, no `[vmem-denied]` line. The game loads PhysX, opens its splash
and a 132×38 "Mortal Kombat" window, and creates a DXVK 3.1.1 D3D9Ex device on the A19 Pro
(1×1 windowed back buffer). It then starts `C:\windows\system32\dxdiag.exe /whql:off /t
…\AppData\Roaming\MKKE\dxdiag.txt` (it reads "Dedicated Memory" from that file). Wine's
dxdiag fails (`err:dxdiag:wWinMain DxDiag information collection failed`) and exits 1;
`ios_leave_sections_at_exit` gives back `fd_cache_mutex` and one more mutex its thread held.
From that moment one game thread spins at 100 % in `libwow64fex.dll`'s
`fex_ios_rpm_lock` (the allocator lock of `patches/rpmalloc-port` 0015), called from
`FEXCore::Allocator::aligned_alloc`; five more threads spin there within seconds, others
wait on a FEX lock that the first spinner owns, and the main thread waits. No frame came in
25 minutes (screen black). Which thread holds the allocator lock is not in the log.

# GOG's MSVC2005 command line, and BioShock's c0000005 (PLA-251, PLA-248)

**Linear:** [PLA-251](https://linear.app/playportdev/issue/PLA-251) (fixed),
[PLA-248](https://linear.app/playportdev/issue/PLA-248) (BioShock, still Broken; cause found).
Decision [0104](../decisions/0104-gog-redistributable-command-line.md). Sweep 2.11n of
[PLA-112](https://linear.app/playportdev/issue/PLA-112). iPhone18,4; runs under
`$PLAYPORT_BUILD/ui-runs/`.

## The command line (PLA-251)

GOG's repository gives Visual C++ 2005 (`MSVC2005`) the string
`/q:a /c:"VCREDI~3.EXE /q:a /c:""msiexec /i vcredist.msi /qb"" "`; the package's cabinet holds
one file, literally named `VCREDI~3.EXE`, which is itself an IExpress package around `vcredist.msi`.

Desktop Wine, a scratch prefix, the installer from GOG's open dependency depot, each line written into
a batch file so that it reaches CreateProcess unchanged:

| Command line after the quoted installer | Exit | WinSxS afterwards |
| --- | --- | --- |
| `/q:a "/c:VCREDI~3.EXE /q:a /c:msiexec /i vcredist.msi /qb "` (what Playport built before) | 0 | Wine's own `vc80` assemblies only |
| GOG's string as it is | 0 | `x86_microsoft.vc80.{crt,atl,mfc,mfcloc,openmp}_…_8.0.50727.762` added |

On the phone (BioShock, GOG 2022341186, Vulkan, 16:9):

| Run | IPA | Prerequisite | Result |
| --- | --- | --- | --- |
| 20261012T221337 (sweep 2.11k) | `d3424096…` | `…vcredist_x86.exe /q:a "/c:VCREDI~3.EXE /q:a /c:msiexec /i vcredist.msi /qb "`, exit 0 after 299 ms | game c0000005 at 14 s |
| 20261012T234554 | `1583ead301c6f4eccb3aa856d1ebc568be4f4099bb04e5a644392466940e552d` | the recorded success had no command line, so it ran once more, with GOG's string as it is: the package started `VCREDI~3.EXE /q:a /c:"msiexec /i vcredist.msi /qb"`, which started `msiexec /i vcredist.msi /qb`; exit 0 after 2051 ms; the game then loaded `winsxs\x86_microsoft.vc80.crt_…_8.0.50727.762\MSVCR80.dll` | game c0000005 at 14 s, same address |

The final IPA (`7c1f3a42d7b6b29a064e060642f923b9d882c4328c36ddbb00c0d73022799e66`) differs from
`1583ead3` only in the wording of the log line for such a rerun. Gates on it: Hollow Knight first frame
9.95 s, main menu (20261013T003042); Portal 2 first frame 5.82 s (20261013T003131).

## BioShock's c0000005 (PLA-248): not the redistributable

The crash is the same with or without the runtime: `Bioshock.exe` reads `[0+8]` at `0x10e5f825`, on the
main thread and on a thread FMOD just started. Static reading of `Bioshock.exe` (FMOD Ex is linked in):
`0x10e5f820` is a spin-lock semaphore operation, reached from FMOD's thread close (`0x10e4b425`) with a
semaphore pointer of 0. FMOD's semaphore create (`0x10e5f9f0`) makes its counter in shared memory:
`CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x14, NULL)`, then
`MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0)`; on a failure it stores 0 and still reports success.

`FEX_TRACE_RIP` (patches/fex 0024) at those addresses, run 20261013T002518 (IPA `1583ead3`): the
first FMOD semaphore gets a mapping handle (`0x1a8`), `MapViewOfFile` returns 0, and ntdll logs
`map_file_into_view_ex unaligned shared mapping 0x703e390000-0x703e391000 not supported`. A writable
shared view of a section smaller than the host's 16 KiB page is refused, so every FMOD semaphore is 0
and FMOD crashes when it closes the first thread that waits on one (during its output start, right after
OpenAL's `wrap_oal.dll` and `avrt` load).

Desktop Proton 10 with the same game files (pulled from the phone) and `-dx9` reaches the main menu,
with and without Creative's OpenAL installed in the prefix (the phone's prefix has it from Baldur's
Gate EE's redistributable); FMOD there loads and closes OpenAL and continues.

The same ntdll line appears in the earlier runs of Tomb Raider: Anniversary (GOG 1202885143, PLA-249:
twice, at the same address each run) and of a local i386 import (Mortal Kombat Komplete Edition).

**Next step:** in `map_file_into_view_ex` (Madeira's `virtual_ios.c`, a `patches/madeira-unix` patch),
log `errno`, the view size and `host_size` for this case, find why `mmap(MAP_SHARED|MAP_FIXED)` of a
0x1000-byte shared section at a 16 KiB-aligned address is not taken or fails, and map such a section
over its whole host page; then BioShock, Tomb Raider: Anniversary and Mortal Kombat Komplete again.

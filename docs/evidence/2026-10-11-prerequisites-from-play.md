# Evidence: prerequisites run once from Play, and House of Golf 2 to its title screen

**Date:** 2026-10-11. **Linear:** PLA-113 (part 2), PLA-67. **Decision:**
[0079](../decisions/0079-prerequisites-from-play.md). **Status:** checked on the phone. Dev IPA
sha256 `b00d4b6e22db1a64a70aa07880486cec9237b971a2171bcd8762e6b2f486a9a6` (sections 2 to 6, built
from the tree before its commit) and `ed58bb43d995a599fc321cbb41a4ac956d0ff4d19bc7deee9d5c2c6d243b1759`
(built from commit 71e099b, the same build records; section 7). Runs are under
`$PLAYPORT_BUILD/agent-notes/pla113c/`.

House of Golf 2 (Epic `48cfbcf29a4441a894d05a203e03dc74`, build 1.2.6.1.0, Unreal Engine 5.4,
installed again for this: 21.9 GB) on Vulkan (vkd3d-proton), 16:9 at 720p.

## 1. The Epic manifest names no prerequisite

The kept manifest (`epic-48cfbcf….manifest`, feature level 21) parsed on the host: `PrereqIds []`,
`PrereqName`, `PrereqPath`, `PrereqArgs` empty. The install record written by this build has
`"prerequisite":{"ids":[],"name":"","path":"","args":""}`. The game ships
`Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe` (50,521,128 bytes, sha256 `3c6d8405…`), which
its bootstrap runs only when the VC++ runtime key is missing; the seed has the key. Hence the UE
rule of decision 0079.

## 2. First Play: the installer runs, then the game

Run `run-hog2-3` (the record removed first over AFC, so the id was not recorded):

```
title: prerequisite: Unreal Engine prerequisites for UEPrereqSetup_x64:50521128-3c6d84054962d95c, not run in this prefix: C:\Games\HouseofGolf2ETnWH\Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe /quiet /norestart
session: prerequisite 1: started, pid 40
session: prerequisite 1: ended, exit code 0 after 824 ms
session: prerequisite 1: registry flush 0 in 0 ms
title: prerequisite: Unreal Engine prerequisites exit 0x00000000 after 0 s; recorded for UEPrereqSetup_x64:50521128-3c6d84054962d95c
```

Marks: runtime started +2.88 s, prerequisite started +2.88 s, prerequisite ended +3.98 s, game
started +4.18 s, first frame +8.31 s. The record afterwards:
`"UEPrereqSetup_x64:50521128-3c6d84054962d95c": {"exitCode": 0, "name": "Unreal Engine
prerequisites", "title": "House of Golf 2", "date": "2026-10-11T03:27:16Z"}`. Its Burn log
(`C:\windows\temp\UE_Prerequisites_(x64)_*.log`, from the run before, same result) shows a
bundle already in this prefix (the manual `dir-uepre` runs of
[2026-10-11-uepre-authenticode](2026-10-11-uepre-authenticode.md) installed it):

```
i001: Burn v3.8.1128.0, … path: C:\Games\HouseofGolf2ETnWH\Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe, cmdline: '"/quiet" "/norestart"'
i101: Detected package: PrereqSetup, state: Present, cached: Complete
i399: Apply complete, result: 0x0, restart: None, ba requested restart:  No
i500: Shutting down, exit code: 0x0
```

A fresh prefix installs the VC++ x86, x64 and MSI packages, as that evidence's run 2 did (6 s).

## 3. Second Play: skipped

Run `run-hog2-4`:

```
title: prerequisite: Unreal Engine prerequisites already ran in this prefix (UEPrereqSetup_x64:50521128-3c6d84054962d95c exit 0x0 on 2026-10-11 03:27:16 +0000); skipped
```

Game started +3.13 s, first frame +7.64 s.

## 4. The installer's session broke the game's Vulkan maps (wine-unix 0027)

On IPA `85d2ff62cdcde4cee5f99ae3e407b4614065db1e348f087eeb66186d3409d5bc` (without 0027), the
first Play (`run-hog2-1`) ran the installer (exit 0) and then the game never drew in 900 s; the
second Play, with the installer skipped, drew at +7.44 s. In the first:

```
005c:err:virtual:map_free_area_inner couldn't map free area in range 0x10000-0x80000000, size 0x16f0000
005c:err:vulkan:win32u_vkMapMemory2KHR NtAllocateVirtualMemory failed
err:vkd3d-proton:d3d12_descriptor_heap_init_data_buffer: Failed to map buffer, vr -1.
```

and a hidden "Error" dialog. win32u's unix init runs at every guest process's win32u load (one
Mach process for all of them), and an i386 child set the session-wide `zero_bits` to its 32-bit
mask, which the x86-64 game then used for its placed Vulkan maps (and would for DIBs). Below
2 GiB iOS maps nothing. wine-unix 0027 takes the mask from the calling thread's process (a
TEB32 marks an i386 child's thread). With it, section 2's first Play has no `alloc-fail` line
and draws.

## 5. House of Golf 2 reaches its title screen

With MFC in the prefix, the c0000005 at about 75 s (the Chroma plugin's null call, PLA-67) is
gone. Screenshots (`run-hog2-5`, `-6`, `-7`, `-8`, `-10`): the UE splash at the first frame, then
black; "LOADING" with a golf-ball icon at the lower right from about +100 s; on the first signed-in
start Epic's consent page in Playport's web panel ("House of Golf 2 wants access to your Epic
Games account"), allowed once with `web:click:Allow`, with the game's 3D room drawn behind it;
on later starts "CHECKING FOR ONLINE DATA, PLEASE WAIT" at +180 s and the title screen (the
House of Golf 2 logo over a 3D kids' bedroom, "PRESS ANY BUTTON TO START") by +240 s, still
there at +618 s (`run-hog2-9`). Footprint about 5.9 GB of the 8 GB limit; around +250 s the
process used 900 to 1500 % CPU (`[xp]` lines). Menus and a round were not
reached in these runs (the driver's pad actions after `play:` did not run).

## 6. Regressions on the committed IPA

- Hollow Knight (`run-hk`): first frame +10.14 s; the main menu (Start Game, Options,
  Achievements, Extras, Quit Game, version 1.5.12620) at +10 s. No prerequisite step.
- Portal 2 (`run-p2`): first frame +6.30 s; the "powered by Source" splash with its copyright
  text. No prerequisite step.
- No other Unreal Engine title is installed on the phone (the others are Unity, Crystal,
  REDengine and their own engines), so no other UE title was played.

Host: `pp test` passed (the session protocol test covers `PP_REQUEST_PREREQUISITE`;
PlayportKit's `PrerequisitesTests`, EpicClientKit's manifest and record round trips).

## 7. The committed IPA

On `ed58bb43…` (`run-hk2`, `run-hk3`, `run-p2b`, `run-hog2-11`): Hollow Knight first frame
+10.23 s (its +10 s screenshot fell in the intro's fade to black; a second run, `run-hk3`, first
frame +9.12 s, shows the main menu at +20 s); Portal 2 first frame +5.71 s, the "powered by
Source" splash; House of Golf 2 `already ran in this prefix; skipped`, first frame +7.56 s, the
title screen at +240 s. Neither Hollow Knight nor Portal 2 logs a prerequisite step.

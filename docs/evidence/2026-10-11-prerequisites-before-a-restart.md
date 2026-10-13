# 2026-10-11: A Play whose prerequisites ran restarts before the game (PLA-172)

**Linear:** [PLA-172](https://linear.app/playportdev/issue/PLA-172).
**Decision:** [0084](../decisions/0084-prerequisites-before-a-restart.md).
**Question:** do a title's prerequisites still take the game's JIT pool head with the fix, and
does the launch stay coherent across the restart?
**Result:** no, they no longer take it. With the fix, Counter-Strike's game started with a head
of 20 MiB after its VC++ 2019 entry ran, the same as a Play with nothing to run. Before the fix
the head was 232 MiB.

## Why a restart

The head is a bump allocator for images. Each pseudo-process takes its own copies of its images
and of ntdll from the bottom of the pool, and a dead process's image ranges are dropped, never
reused (ARCHITECTURE.md, "JIT pool use"). The prerequisites' `cmd`, `conhost`, `start` and
installer processes therefore stay in the head for the rest of the session. A process runs one
session (decisions 0029, 0030), so the game's fresh pool comes from a fresh process: the restart
Playport already makes after every game.

## Phone

All runs are on the iPhone18,4 with iOS 27.0, a 512 MiB pool and `pp ui --wait 180`, using
`--until "mark:game started+20"` for Counter-Strike (`app-10`, installed for this check and
uninstalled afterwards). Run directories are under `.work/ui-runs/`.

### Before: IPA `af249291926c44b7f8f5233cab0a17dcf8ed0c84aa881a8db4a53ab97284e7d2` (87bdc00)

- **Install:** `20261011T112459`.
- **Play with prerequisites:** `20261011T112614`.
  - Both VC++ 2019 entries ran again, because the build had changed since they were recorded
    (decision 0083).
  - x86 ended with 0x666 (1638, downgrade refused). x64 ended with 0, recorded, and its
    HasRunKey written: this time it installed.
  - The game started at +7.67 s, 0.26 s after the second entry ended. The pool line at +8.31 s
    read `head_mb=232 head_live_mb=20 head_free_mb=213 tail_mb=113 room_mb=168 children=6`.
  - The game then hit a host fault (`unrecoverable host fault misdelivered to guest`, then the
    process was ended), and the app exited. This is Counter-Strike's known fault (PLA-164);
    whether the shared session made it worse is not known.
- **Play without prerequisites:** `20261011T112735`. Both entries were recorded by this build
  and skipped. The game started at +4.64 s with `head_mb=27 room_mb=453` at +20 s.

### After: IPA `e5bd142e7435d2c4d7893064bef38d2c288fb14539e66eec127c63327f1066e0` (this change)

**Play with prerequisites:** `20261011T113752`. Only x86 ran again: x64 had succeeded.

- **Old process (pid 30338):**
  - `prerequisite: Steam x86 14.28.29334.0 … failed with another build (… by build 16a7e93a1bd1381b); build 68c8f731b36d2759 runs it again`
  - It ended with `exit 0x00000666 after 1 s; recorded …`, then
    `prerequisites: 1 ran; Playport restarts and the game starts in the new process`.
  - The mark `+5.62 s prerequisites done; restarting for the game` came with the pool line
    `head_mb=119 head_live_mb=20 head_free_mb=100 tail_mb=97 room_mb=297 children=3` (one entry).
  - `title: prerequisites ran: launch=restart prerequisites=1`. There was no `title: done` line.
  - `ui: continuing after the restart: the Play, then no actions`.
- **Restart:** `asking CoreDevice to restart pid 30338`, and the app service answered at +157 ms.
- **New process (pid 30345):**
  - `restart: new process pid 30345, 388 ms after pid 30338 asked; continuing the Play of Counter-Strike (app-10)`
  - `library: play Counter-Strike (app-10): continued after its prerequisites ran before the restart; none run now`
  - JIT was acquired in 2.56 s.
  - `title: at the game's start: pool: size_mb=512 head_mb=20 head_live_mb=20 head_free_mb=0 tail_mb=17 room_mb=476 … children=0`
  - At +20 s the result event's pool read `head_mb=27 room_mb=453`.
  - The run (nonce `dc5a2a5f`) followed the Play across the restart and ended at
    `mark:game started+20`.
- **Time:** the game started 9.2 s after the prerequisite ended. Before the fix, the game started
  0.26 s after the last prerequisite ended.
  - Restart request to new process: 0.4 s.
  - The new process's library scan and Steam list, to the continued Play: about 3.9 s.
  - Play to game start: 4.9 s, including JIT.
- **Screens:**
  - `screen-0007s.png`: the launch screen with the game's art, the bar and "Installing
    Counter-Strike's prerequisites…".
  - `screen-0009s.png`: the iOS Home Screen while the process is replaced. This happens on every
    restart.
  - `screen-0011s.png`: the game's art, its name and "Starting Counter-Strike…" in the new
    process.

**Play without prerequisites:** `20261011T113936`.

- `prerequisite: Steam x86 … already ran in this prefix (… exit 0x666 …); skipped`.
- There was no restart.
- `title: at the game's start: pool: … head_mb=20 … room_mb=476 … children=0`, then `head_mb=27`
  at +20 s.

**Pool head at the game's start**

| | With prerequisites | Without |
| --- | --- | --- |
| Before (af249291) | 232 MiB, +0.64 s after the game started (pool line at +8.31 s) | 27 MiB at +20 s |
| After (e5bd142e) | **20 MiB** (`at the game's start`); 27 at +20 s | 20 MiB (`at the game's start`); 27 at +20 s |

**Regressions on e5bd142e**

| Title | Run | First frame | Screen |
| --- | --- | --- | --- |
| Hollow Knight (`app-367520`) | `20261011T114124` | +9.77 s | main menu at +10 s |
| Portal 2 (`app-620`) | `20261011T114224` | +5.72 s | Source splash |
| House of Golf 2 (`epic-48cfbcf2…`) | `20261011T114323` | +7.32 s | UE prerequisite already ran, skipped, no restart |

**After the runs:**

- Counter-Strike was uninstalled (`20261011T114044`).
- `Games/Steamworks Shared` again holds depot 228988 (VC++ 2019, 22.9 MB), downloaded by the
  "before" Play.
- The prefix's record has x86 VC++ 2019 failing (0x666) under build 68c8f731b36d2759, and x64
  succeeding with its HasRunKey written.

## Host checks

- PlayportKit:
  - `PrerequisitesTests.testAPlayRestartsBeforeItsGameOnlyAfterAPrerequisiteRan`
  - `PrerequisitesTests.testAPlayContinuationRoundTripsAndDecodesWithDefaults`
  - `PlayportKitTests.testTheRestartAfterPrerequisitesNamesTheGame`
- `pp check` compiles both the dev and the release variants.

## Not checked

- Inside LiveContainer, or JIT from another app with no pairing: there is no restart, and the
  old path stays.
- A restart request that fails (LocalDevVPN down).
- An Epic title played offline with prerequisites pending.
- Quit game during the prerequisites. The code path is unchanged.
- A prerequisite at its 300 s limit. The close path is unchanged; the restart follows it.
- The release variant on the phone. Owner sign-off applies (decision 0068).

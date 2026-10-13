# 2026-10-13: an exited i386 child's window teardown, and the threads of a terminating process

**Linear:** [PLA-230](https://linear.app/playportdev/issue/PLA-230) (the threads of a
terminating process busy in FEX run on), [PLA-229](https://linear.app/playportdev/issue/PLA-229)
(a killed thread parked in an in-process wait is never ended), the i386 childabort hang seen in
sweep 14b; all under [PLA-200](https://linear.app/playportdev/issue/PLA-200) and
[PLA-112](https://linear.app/playportdev/issue/PLA-112). The owner was away. The conservative
option was taken at each choice and is recorded below.

**Changes:**

- `patches/madeira-unix` 0139: a WoW64 window torn down when its last thread is reclaimed (its
  owner has exited) no longer closes the section handles of its image views.
- `patches/madeira-unix` 0140: the abort rings FEX's suspend doorbell for every killed thread,
  also the threads of a terminating process (madeira-unix 0130 rang it only for a thread killed
  alone). `ios_abort_thread_in_guest_code` loses its `ring_doorbell` argument.
- `tools/compat/probes/childabort.c`: the phases `killwait` and `crashwait`.

**IPA:** `Playport-26.5-c5d157c2.ipa` (dev), sha256
`c5d157c2a6e0f99e9b36c417e0c4e18b698c63b7655dda088ecfa8a58b83d339`, installed in place. Two
intermediate IPAs also carried a PLA-229 change that was not kept (below). Run directories are
under `$PLAYPORT_BUILD/agent-notes/speedrun/s14c/runs/`.

## The i386 childabort hang after its crash phase (sweep 14b)

**Before.** In 14b, `dir-childabort-14-i386` (phases `dxdiag crash`) ended `done ok` in 2 of 3
runs on IPA `408be0ea…` and in 0 of 3 on `91e30448…` (with madeira-unix 0138). Every one of
those six runs, the passing ones included, showed the same sequence after crash child 0:

- the next child's `virtual_map_main_module` failed with `c0000008`, and the parent's
  `CreateProcess` with error 6 (`crash child 1 exit code 0xfffffffe`);
- just before it, four `[bad-close]` lines for handles `0xc`, `0x28`, `0x2c` and `0x24`, then
  `[wow64-window] torn down base=…`, on the new child's thread;
- in the runs that hung or ended the app, later a parent worker's
  `wine client error: write: Bad file descriptor` and
  `[section] thread abort … (fd_cache_mutex)`.

**Cause.** An i386 child's image views keep a duplicate of their section handle
(`view->wow64_mapping`), and `free_view` closes it. A crashed child exits while its busy workers
still run, so its window is only retired, and it is torn down when the last of those threads is
reclaimed. That happens on whichever thread reclaims: here the next child's thread reserving its
own window. `NtClose` then went to the server as that thread's process and closed the next child's
handles of the same numbers. `0x10`, its main image's section, was valid there and was closed, so
the map failed with `c0000008`. When a parent thread reclaimed, the parent lost its own handles:
the cached descriptors behind them closed, and a later server write failed with EBADF under
`fd_cache_mutex`.

**Is madeira-unix 0138 implicated? No.** The sequence above is the same in all six 14b runs, on
both IPAs. 0138 only changes which pool ranges image copies get (none of these runs reused or
dropped a range). The 3/3 against 1/3 is how often the later EBADF happened to hit; that depends on
which thread reclaimed the window. No separate A/B build was made: the cause was found first, and
the fix was checked on its own (below).

**Fix (madeira-unix 0139).** The server closes an exited process's handles itself. A window torn
down at its last thread's reclamation now deletes its views without `NtClose`
(`ios_wow64_delete_dead_owner_views`). The owner's own initial thread, tearing its window down at
its exit, still closes them.

**Phone, final IPA `c5d157c2…`, `dir-childabort-14-i386`, phases `dxdiag crash`:**

| Run | Crash children | `[bad-close]` | EBADF | Result |
| --- | --- | --- | --- | --- |
| `v3-i386-def1` | `0xc0000005` ×3 | 0 | 0 | `done ok` |
| `v3-i386-def2` | `0xc0000005` ×3 | 0 | 0 | `done ok` |
| `v3-i386-def3` | `0xc0000005` ×3 | 0 | 0 | `done ok` |

The intermediate IPAs, which had 0139 and 0140, gave the same result in six more runs
(`i386-default-1..3` on `c9dbe78b…`, `v2-i386-def1..3` on `7fa60e30…`): 9 of 9 `done ok`, no
`CreateProcess` failure, no `c0000008` and no EBADF. Before, 0 of 6 had a clean crash phase.

## PLA-230: the threads of a terminating process

**Before.** madeira-unix 0130 rang the doorbell only for a thread killed alone. The threads of a
terminating process kept 0127's abort, which ends a thread only when it finds it in translated
code holding nothing. A thread busy in FEX itself ran on for up to about a minute after its
process had exited. Ringing their doorbell had been tried in 2.10d (`i386-ct4`, `i386-ct5`) and
broke the next child's start with exactly the 14b sequence above: `CreateProcess` error 6,
`c0000008`, then EBADF. Those threads ended earlier, so their window was torn down on the next
child's thread.

**Fix (madeira-unix 0140).** With 0139 in place, every killed thread gets its doorbell rung.

**Phone:**

- `v3-i386-def1..3`: the crashed children's threads end by the time the parent sees the exit
  code. For example, in `v3-i386-def1` the windows are `retired` with 3, 2 and 3 threads left, and
  `[async-abort] … ended after 2 tries` follows. The window is `torn down` before the next child
  starts, and `[async-abort]` reports `left running 0`.
- `v2-i386-ct` (`crash terminate`, IPA `7fa60e30…`, the same 0139 and 0140): crash children
  `0xc0000005` ×3 and `done ok`. The `terminate` phase still ends every terminated worker.
- `v3-i386-kw` (`killwait crashwait`): `done ok`.

**Not covered.** A thread of a terminating process that is parked in an in-process wait is still
not ended (PLA-229, below). In `v3-i386-def3` the last child's window kept 3 of its threads until
the end of the run (`threads created=22 reclaimed=19`).

## PLA-229: a killed thread parked in an in-process wait

**Before.** The new `killwait` phase parks 32 threads in `RtlWaitOnAddress` and
`SleepConditionVariableCS`, then terminates them. On the old IPA `91e30448…` (`before-i386-kw`),
every CHECK passed, so the probe cannot see the problem from Windows:

- A killed waiter stays parked until something wakes its address.
- The probe then wakes it, and the doorbell (0130) ends it at its first block.

The `crashwait` children's two parked waiters are never woken. Each child's window stays retired
for the rest of the session, and a new 4 GiB window is reserved for every later child
(`base=0x7238950000`, then `0x73ffff0000`; `threads created=50 reclaimed=44`).

**Tried, and not kept.** It was built into IPAs `c9dbe78b…` and `7fa60e30…`, and its patches are
kept in the build area (`s14c/pla229/`):

- The abort finds the killed thread in `NtWaitForAlertByThreadId`, marks it, and wakes it.
- The wait returns `STATUS_THREAD_IS_TERMINATING` once.
- `RtlWaitOnAddress` takes its entry off the futex queue. In the second form, it also passes on a
  wake it had already been given. It then waits again, and that call ends the thread.

What happened on the phone:

- x86-64 `killwait crashwait` passed (`after-x64-kw`, `v2-x64-kw`).
- `[killed-wait]` showed 32 of 32 killed waiters ended in their wait, with no fault walking the
  shared queue.
- **But the i386 `killwait crashwait` hung in 3 of 4 runs** (`after-i386-kw`, `after-i386-kw2`,
  `v2-i386-kw2`; `v2-i386-kw` passed):
  - The parent's busy workers, and the next crashed child's main thread or the parent's main
    thread, parked in `NtWaitForAlertByThreadId` for good.
  - The likely cause is a lock that pseudo-processes share. A child's PEB is a copy of the
    session's, which shares the 64-bit process heap. A child's thread that dies in the middle of
    acquiring such a lock leaves it inconsistent for the other processes.
- The same phases passed without the change (`before-i386-kw`, and `v3-i386-kw` on the final IPA).

So it was dropped. Ending a killed thread in its wait needs those locks to be per process, or the
lock's own wait path to undo the thread's place in it. Neither fits this sweep.

## Gates (final IPA `c5d157c2…`)

| Run | What | Result |
| --- | --- | --- |
| `gate-hk` | Hollow Knight `app-367520 --until first-frame+10 --shot` | first frame +10.03 s; main menu (Start Game, Options, Achievements, Extras, Quit Game) |
| `gate-p2` | Portal 2 `app-620 --until first-frame+10` | first frame +5.93 s |

- No `pp perf` (the owner's testing budget).
- Host: `pp test --quick` passed, and `pp names` and `pp secrets` are clean.
- Desktop Wine 11.18 passes `killwait crashwait` for both builds of the probe.

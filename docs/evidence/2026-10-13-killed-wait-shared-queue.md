# 2026-10-13: a killed i386 thread ends in its own wait; the shared futex queue it broke

**Linear:** [PLA-312](https://linear.app/playportdev/issue/PLA-312) (child pseudo-processes
share the session's 64-bit heap and locks), [PLA-229](https://linear.app/playportdev/issue/PLA-229)
(a killed thread parked in an in-process wait is never ended); under
[PLA-200](https://linear.app/playportdev/issue/PLA-200) and [PLA-112](https://linear.app/playportdev/issue/PLA-112).
It follows [2026-10-13-killed-threads-teardown.md](2026-10-13-killed-threads-teardown.md) (sweep 14c),
whose PLA-229 design hung the i386 childabort parent in 3 of 4 runs. The owner was away; the
conservative option was taken at each choice.

**Changes:** `patches/madeira-unix` 0142, `patches/wine-unix` 0037 and `patches/wine-pe` 0043
(the 14c design, kept in the build area), with one change in 0142: only an i386 program's own wait,
made through WoW64, ends a killed thread (`ios_alert_wait_from_wow64`).

**IPA:** `Playport-26.5-c17bae5b.ipa` (dev), sha256
`c17bae5bd66110ac6b60e9f46a7b61075cb517943c663b85cc2ff6f9eccde2bf`, installed in place. One
diagnostic IPA before it (`6181f943…`, the 14c design plus backtraces). Run directories are under
`$PLAYPORT_BUILD/agent-notes/speedrun/s14e/runs/`.

## The lock the stuck threads wait on

Two hung runs were read: 14c's `v2-i386-kw2` (IPA `7fa60e30…`) and `diag-i386-kw1` (IPA
`6181f943…`, the same design with backtraces of every ended waiter and, once a wait passes 45 s, of
every thread). Both hung the same way after crashwait child 0 or 1:

- The parent's busy workers `b8` and `bc` waited in `NtWaitForAlertByThreadId` on
  `libwow64fex.dll`+`0x4141f2`. That is `owners` of FEX's WoW64 `ThreadCreationMutex` (a
  `std::mutex`, a Wine SRW lock, at RVA `0x4141f0`). Its word was `00010005`: held exclusively, two
  waiters.
- The other two workers and the main thread were not parked. They ran at 100 % in the parent's
  64-bit ntdll: `RtlWakeAddressSingle` (`+0x6e654`, `+0x6e664`, called from FEX's
  `WritePriorityMutex::unlock` and another FEX caller) and `RtlWaitOnAddress` (`+0x6f984`). The main
  thread also waited for the parent's i386 process heap lock, which one of the spinners held:
  `RtlpWaitForCriticalSection section 00030078 "...main process heap section" ... blocked by 00c4`.
- They spun on a futex queue. Since ml441 every ntdll copy of every pseudo-process uses one futex
  table in the unix library. A queue whose list links into a dead thread's stack sends a walker
  round for ever while it holds the queue's spin lock.

**Who left the entry.** The `[killed-wait]` backtraces in `diag-i386-kw1` give the caller of each
ending wait:

- The 32 parent waiters of `killwait`, and some crashwait waiters, ended in a call from `wow64.dll`
  (`+0x19da8`, `wow64_NtWaitForAlertByThreadId`). That is the i386 program's own wait, as designed.
- Crashwait child threads `d0` and `ec` ended in a call from the child's **64-bit** ntdll at `+0x6f824`.
  That is the *first* `NtWaitForAlertByThreadId` call of the 64-bit `RtlWaitOnAddress`, made after
  its entry was queued.
- The cause: each such thread had been told `STATUS_THREAD_IS_TERMINATING` in its i386 wait.
  Before it made the second i386 call, FEX ran 64-bit code: it translates the i386
  `RtlWaitOnAddress` that just returned, and that code waits on FEX's locks. The thread's "told"
  state ended it in that host wait, with its entry still in the shared queue. It may also have held
  FEX's `ThreadCreationMutex` or the heap lock.

**What is shared, and what is not.**

- *i386.* Each i386 pseudo-process maps its own 64-bit `ntdll.dll`, `wow64.dll` and
  `libwow64fex.dll` (separate `[jit-pool] image` copies per child), so its process heap, loader lock
  and FEX locks are its own. The stuck `ThreadCreationMutex` and heap lock are the *parent's*, and the
  parent's threads only stopped because the *shared* futex table was broken. The 64-bit heap is not
  shared for i386 programs.
- *x86-64.* Each x86-64 child maps its own `libarm64ecfex.dll`, but all of them run on the session's
  one ARM64EC `ntdll.dll` (loaded once). Its process heap (PEB copied from the session's) and the
  ntdll's static locks are therefore shared with the session. 14c's reading of the PEB copy applies
  here.

**Decision.** Give no pseudo-process its own heap or locks. That would change process-sharing
semantics, and the hang did not come from them.

- A killed thread now ends in its wait only when the wait is an i386 program's own, made through
  WoW64. In that case FEX holds nothing (the thread is in a guest syscall), and the i386 locks and
  queue entry belong to the program.
- The test is `ios_alert_wait_from_wow64`: the syscall stub's caller (`lr`) is in another image than
  the stub (`pc`), on a thread with a WoW64 TEB. The abort checks this before it marks and wakes a
  thread, and the wait checks it before it acts on the mark.
- Every other in-process wait is left to wake as before 14c: the 64-bit ntdll's own waits for an
  i386 thread, and every wait of an x86-64 thread. Those waits may hold FEX's locks, or the loader or
  heap locks that x86-64 pseudo-processes share with the session.
- The `[killed-wait]` line now names the caller.

## Phone, final IPA `c17bae5b…`

**i386 `childabort killwait crashwait`** (`fix-i386-kw1..4`): **4 of 4** `done ok`, and every CHECK
passes.

| Run | `[killed-wait]` (all from `wow64.dll`+`0x19da8`) | Child windows | Threads created / reclaimed |
| --- | --- | --- | --- |
| `fix-i386-kw1` | 38 | all torn down | 50 / 50 |
| `fix-i386-kw2` | 38 | all torn down | 50 / 50 |
| `fix-i386-kw3` | 36 | child 1's stays retired | 50 / 48 |
| `fix-i386-kw4` | 39 | all torn down | 50 / 50 |

In `fix-i386-kw3`, child 1's two waiters were told (their i386 wait returned `c000004b`) but never
made the second call. They stayed parked, as every killed waiter did before 14c, and the next child
reserved a new window. Nothing else hung.

Before, on 14c's design: hung in 3 of 4 runs (14c), and in 1 of 1 (`diag-i386-kw1`).

**i386 `childabort dxdiag crash`** (`fix-i386-def1`): `done ok`, crash children `0xc0000005` ×3, and
`threads created=22 reclaimed=22`. 14c's final IPA had `22/19` here.

**x86-64 `childabort killwait crashwait`** (`fix-x64-kw1..2`): `done FAIL` in 2 of 2, on
`CHECK killwait critical section free`. All crash children end `0xc0000005`, with no hang and no
`[killed-wait]`.

- This is the PLA-229 bug as it stands for x86-64. A killed condition-variable waiter stays parked.
  When the probe wakes it, it takes the critical section back, and the doorbell then ends it while it
  holds the section.
- The x86-64 path is the one before 14c (the WoW64 test is false for it). The phases first ran
  without the 14c design here, so the failure is not new; 14c's x86-64 passes were with the
  dropped design.

**Gates** (final IPA):

| Run | What | Result |
| --- | --- | --- |
| `gate-hk` | Hollow Knight `app-367520 --until first-frame+10 --shot` | first frame +8.93 s; screenshot: main menu (Start Game, Options, Achievements, Extras, Quit Game) |
| `gate-p2` | Portal 2 `app-620 --until first-frame+10` | first frame +6.03 s |

## What is left

- **x86-64 killed waiters (PLA-229).** They need a safe point to end that the waits themselves do
  not give. Their ntdll's locks are shared with the session, and a wait can be inside FEX.
  - One way: end such a thread in its wait only when its stack shows the wait came straight from
    translated code through the waiting functions themselves (`RtlWaitOnAddress`,
    `RtlSleepConditionVariable*`, `RtlEnterCriticalSection`, `RtlAcquireSRWLock*`, kernelbase's
    `WaitOnAddress`), and FEX's TEB stamps show no lock.
  - The other: give each x86-64 pseudo-process its own ntdll copy, as i386 ones have.
  - Either one is a separate change.
- **A told i386 thread that parks in a host wait** before its second call stays parked, as in
  `fix-i386-kw3`.

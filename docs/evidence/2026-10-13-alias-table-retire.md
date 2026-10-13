# 2026-10-13: A dead process's images leave every emulator's alias table (PLA-310)

**Linear:** [PLA-310](https://linear.app/playportdev/issue/PLA-310) (sweep 14d of
[PLA-112](https://linear.app/playportdev/issue/PLA-112)).
**Change:** `patches/fex` 0039 and `patches/madeira-unix` 0141.
**Questions:**
- Why does the tenth x86-64 child of the `poolstress` probe die at a wild pc?
- Can a title start many processes without filling FEX's image alias table?

**Result:**
- Each live process's emulator now loses a dead process's images when that process's pool
  copies are freed. 25 rounds of the probe (50 children) ran clean, with no full table, where
  10 rounds had failed.
- A full table no longer logs from inside the push. That log was the likely cause of the wild pc:
  it made a nested unix call while the child was inside one. A refused alias is now reported by
  Wine.

## The table and why it filled

Wine copies every PE image into the JIT pool and pushes the pair (PE range, pool copy) to every
process's emulator through `BTCpu64IosAddAliasMapping`. Each emulator has its own 256-entry
`IosAliasEntries`. `ExitFunctionEC` (Module.S) uses it to turn the target of an x64→EC call into
the pool copy. Since madeira-unix 0044 there is one callback per pseudo-process (`ios_alias_cbs`).

When a process dies, `ios_jit_reclaim_process` tombstones its images in Wine's own table
(`ios_jit_mappings`). It never told the emulators. FEX drops an entry only when a new image
overlaps its PE range. A child's images sit at new PE addresses, so its entries stayed in the
parent's and the session's tables. Each round of the probe left 25 there: 20 from the x86-64
child and 5 from the i386 one. After nine rounds those tables were full.

The same entries were also wrong once 0138 reused a dead copy's pool range for another image.
FEX's reverse lookup (pool → PE, oldest first) then found the dead owner's entry for the new
copy's addresses.

## Why the child died

On IPAs `408be0ea` (without 0138) and `8ced8eb1` (with it), round 10's x86-64 child failed the
same way. It loaded combase, and Wine pushed the alias from that child's thread inside
`mprotect_exec`, which runs within its `NtMapViewOfSection` unix call. Two emulators were full,
and each logged `[jit-alias] ml1106 IosAliasEntries FULL (256 entries): image 0x73e5660000+0xc0000
NOT registered`. Those two lines are the first FEX `LogMan` output from inside a push.

FEX's message handler writes through PE ntdll's `__wine_dbg_output`, a unix call. Made from
inside another one, it can overwrite the outer call's syscall frame. When `virtual_map_image`
returned, the child ran at `pc = lr = 0xfffffffef7768be7` (`UNMAPPED AT ENTRY`) and ended
with `c0000005`. A refused alias alone only costs speed: calls into that image take the
exec-fault redirect.

This cause is inferred from the order of events, the same in both runs. It was not proven by a
run that fills the table without the log.

## The change

- **FEX 0039 (`IosJitAlias.cpp`):**
  - A size of 0 retires the entry with that PE base and pool copy. A retired entry has size 0,
    so it matches no lookup, and the next image added reuses the slot.
  - The export returns 1 when the alias is in the table and 0 when it is refused (the table is
    full).
  - Nothing on this path logs any more.
- **madeira-unix 0141 (`virtual_ios.c`):**
  - `ios_jit_reclaim_process` collects the images it tombstones. After it releases the pool lock,
    it retires each one from every live emulator (`ios_alias_retire_all`). The RECLAIM line now
    says how many (`N mappings tombstoned (N retired from live emulators)`).
  - The dead process's own emulator was already removed from the callback list
    (`ios_alias_forget_process`). A late thread of that process keeps its translations, and no
    other process runs its images. Reclaim runs before the 3 s reuse grace ends, so a range is
    out of every table before 0138 can hand it on.
  - A refused alias, from a push or from a new emulator's initial drain, is logged by Wine:
    `[jit-alias] PLA-310 emulator of peb=… FULL: image …` (8 lines at most).

The callback type changes from `void` to `int` on both sides. Both are built from this tree.
PE-side ntdll only passes the pointer on and never calls it.

## Phone

All runs are on the iPhone18,4 with iOS 27.0, plugged in. Run directories are under
`$PLAYPORT_BUILD/agent-notes/speedrun/s14d/runs/` (s14b's for the old runs).

| Run | IPA | Rounds | Children failed | FULL lines | Result |
| --- | --- | --- | --- | --- | --- |
| s14b `before10` | `408be0ea…` | 10 (`10 3000`) | round 10's x86-64 child, `c0000005` | 2 | stopped at the wild pc |
| s14b `final10` | `8ced8eb1…` | 10 (`10 3100`) | round 10's x86-64 child, `c0000005` | 2 | stopped at the wild pc |
| `ps25` | `01e8c4ad…` | 25 (`25 3000`) | 0 of 50 | 0 | `poolstress: parent: 50 children, 0 failed`, `done ok`, exit 0 after 158 s |

`ps25` in detail:
- Each x86-64 child's RECLAIM retired 20 aliases and each i386 child's retired 5. At the end the
  parent's own 17 were retired too.
- There were no `UNHANDLED` and no `BORN NON-EXEC` lines.
- The pool head stayed at 98 MiB (room 366), as in 14b.

### The other probes and gates (same IPA)

| Probe or title | Run | Result |
| --- | --- | --- |
| `childabort.exe` x86-64 (`dir-childabort-14`, dxdiag and crash) | `ca-x64` | `childabort: done ok`, 0 stale |
| `childabort-i386.exe` (`dir-childabort-14-i386`, dxdiag and crash) | `ca-i386` | `childabort: done ok`; no `[bad-close]`; the same `c0000008` and `UNHANDLED` counts as 14c's `v3-i386-def1` |
| `protect.exe exec-rw-reuse` (`dir-protect 2`) | `protect` | every phase ok, `protect: done ok` |
| Hollow Knight (`app-367520`, `--until first-frame+10 --shot`) | `gate-hk` | first frame at +10.00 s; the screenshot shows the main menu (Start Game, Options, Achievements, Extras, Quit Game); head 84, tail 161, room 268 |
| Portal 2 (`app-620`, `--until first-frame+10`) | `gate-p2` | first frame at +5.56 s; head 27, tail 161, room 325 |

IPA `Playport-26.5-01e8c4ad.ipa`, sha256
`01e8c4ad13aeaacaba6ee5b33b7ed80d49a62573e57231a954425017847353a3`.

## Host

`./pp test --quick` passes.

## Not done

- **Pushing only what an emulator needs.** Every image still goes to every emulator. The parent
  carries its live children's images until they die. A title that keeps more than about ten
  x86-64 processes alive at once could still fill a table. A refusal is now logged and costs only
  speed.
- **The full-table path was not driven on the phone with this change.** The table no longer
  fills in the probe, so the new refusal log was not seen.

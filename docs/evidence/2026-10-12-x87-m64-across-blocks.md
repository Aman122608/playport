# Evidence: a `fild m64` integer stays exact into a later FEX block (PLA-235)

**Date:** 2026-10-12. **Kind:** phone runs (dev app) of a probe, desktop Wine run of the probe.
**Linear:** [PLA-235](https://linear.app/playportdev/issue/PLA-235), after
[PLA-202](https://linear.app/playportdev/issue/PLA-202) (`patches/fex` 0025); title
[PLA-187](https://linear.app/playportdev/issue/PLA-187) (Neverwinter Nights Diamond).
**Changes:** `patches/fex` 0031, the probe `tools/compat/probes/x87m64.c`. The owner was away, so
the conservative option was taken at each choice below. Run directories are under
`$PLAYPORT_BUILD/ui-runs/`.

**IPAs:** before, `Playport-26.5-12062a00.ipa` (dev, sha256
`12062a00f1624405dc4fa8e529eab303ed4b29ef6036e56bcafb4428702c8bbd`, 16ffed2); after,
`Playport-26.5-13f8dda9.ipa` (dev, sha256
`13f8dda954c39a5e3ea5999ecd457015b97ccef906984b8837485d475b1b43f7`), built from 3b942dd with this
change and the ANSI code pages of [2026-10-12-gog-inno-ansi-code-page](2026-10-12-gog-inno-ansi-code-page.md),
installed in place.

## The bug

[2026-10-12-gog-support-setup](2026-10-12-gog-support-setup.md): at the default reduced x87
precision (decision [0048](../decisions/0048-x87-reduced-precision-global.md)), GOG's Inno Setup
5.5.5 loader (i386, Delphi) opened `D:\Games\NWN Diamond\galaxy_nwn_diamond_2.1.0.210exe` and
showed `Path not fpund.`. Delphi's `Move` copies 8 bytes with `fild qword` and `fistp qword`: for
9 to 32 bytes it loads the last, first and second 8 bytes, jumps on the count and stores them in
reverse; for more it loads the first 8 bytes before a loop and stores them after it. UTF-16 text
such as `C:\G` is an integer near 2^54, which a double keeps only to a multiple of 4.

`patches/fex` 0025 keeps the integer exact only while the `fistp` is in the same FEX block as the
`fild`. A branch target starts a new block, which reads the x87 stack back from the thread's
context, where reduced precision keeps the double alone.

## The change: `patches/fex` 0031

The x87 stack pass (`x87StackOptimizationPass.cpp`) writes each register's context slot at a
block's end or on its slow path. Each slot is 16 bytes; reduced precision used the low 8.

- Reduced precision now stores the whole slot. For a value a `fild m64` pushed and nothing changed,
  the upper half is the loaded integer (one `ins` before the store). For any other value it is
  whatever the register's upper half holds, zero for a scalar result.
- A `fist m64` on the slow path, of a top unchanged since the block began, loads that half,
  converts it to a double and uses it when that equals the double on the stack (`scvtf`, `fcmeq`,
  `bsl`). Below 2^53 such an integer is the value itself; above, it is the `fild`'s integer, or
  another integer that rounds to the same double, as reduced precision allows. Otherwise the
  double is converted as before.
- A `fild m64` on the slow path keeps its integer for a `fist m64` later in that block.
- Moves on the slow path (FXCH, `fld st(i)`) do not carry the integer; such a value goes through
  the double as before. 80-bit x87 is unchanged.

A FEX asm test, `unittests/ASM/X87_F64/FILD_FISTP_I64_Exact_Blocks_F64.asm`, covers a branch, both
forms of Delphi's `Move` and a changed value. It was not run: FEX's harness needs an ARM64 Linux
host. No desktop FEX was available, so the probe on the phone is the check.

## The probe

`x87m64.c` gains `fild;jcc;fistp` and `fild;jcc;fisttp` (a conditional jump between the two), and
Delphi's two `Move` forms copying 24 and 48 bytes, for the UTF-16 path `C:\G…` as Inno copies it,
the fixed values and every sixth of the 100,000 pseudo-random values. Under desktop Wine (x87 with
a 64-bit significand) every check is `ok`.

| Run | IPA | Outcome |
| --- | --- | --- |
| `20261012T170423` | `12062a00…` (before) | `done FAIL`: every within-block form `ok`; `fild;jcc;fistp` of `0x84280002320300a8` gave `0x8428000232030000`; the small `Move` of `C:\G`, `mes\`, `N NW` gave `0x0047005c003a0044` (`D:\G`), `0x005c00730065006c` and `0x0057004e00200050`, the loader's corruption; 232,093 failures in the random values |
| `20261012T170817` | `13f8dda9…` (after) | `done ok`: all forms and both `Move` copies exact for the fixed and random values; the control `fild;+0;fistp` still `0x8428000232030000`, so the run was at reduced precision |

## The title

On `13f8dda9…` NWN Diamond's first Play ran GOG's support setup at the default x87 setting
(`20261012T170848`): the loader found itself, no `Path not fpund.` box appeared, the setup exited
`0` and the game started with its `binkw32.dll`. The rest of that replay is in
[2026-10-12-gog-inno-ansi-code-page](2026-10-12-gog-inno-ansi-code-page.md).

## Gates and cost

On `13f8dda9…`: Hollow Knight (`app-367520`, `20261012T171245`) first frame +9.9 s, its main menu
at the stop; Portal 2 (`app-620`, `20261012T171335`) first frame +5.8 s. Both `ok`. No `pp perf`
run (owner's testing budget). The added code is a 16-byte store in place of an 8-byte one, an
`fmov` and `ins` for a `fild m64` value alive at a block's end, and five instructions for a
`fistp m64` on the slow path; no other x87 operation changes.

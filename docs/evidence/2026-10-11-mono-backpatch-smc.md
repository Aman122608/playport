# Duck Paradox with SMC tracking off: a real guest call to 0, not a FEX link

**Date:** 2026-10-11. **Branch:** `dev/pla-152-gdi-windows`. **Linear:** PLA-115 (found by
PLA-52). **Title:** Duck Paradox (GOG `1816031813`), Unity, Mono `mono-2.0-bdwgc.dll`.

**Result:** diagnosis only, no fix. PLA-115 described the crash as a FEX block
branching to a null host address after crossing into a code page Mono had just
written. That is wrong. The branch is a genuine guest `call r11` with `r11 = 0`, in a
Mono method whose `mov r11, imm64` call templates in one 16 KiB page are still 0 when
the method runs. FEX translates what is in memory. The compatibility default
`runtime` `env.MADEIRA_KEEP_SMC=1` ([2026-10-09](2026-10-09-duck-paradox-smc.md)) stays.

## How it was found

Every run was on a diagnostic FEX build (two throwaway patches on top of
`patches/fex` 0024, never committed), with the compatibility default removed:

1. The JIT's inline L1 lookup for an indirect branch takes an entry whose tag
   matches and whose host code is 0. An empty entry is `{0, 0}`, so a branch to guest
   RIP 0 matches it and branches to host 0. Wine then builds the guest context from
   the JIT's registers, which is the corrupt context Mono's handler "wrecks" in the
   2026-10-09 log. The diagnostic build sends such a hit through the dispatcher, which
   spills the guest state. The dispatcher then refuses the RIP:

       I 2C [iOS] CompileBlock: REFUSING low/invalid RIP=0x0
       [low-rip] rip=0x0 rsp=0x702d3aef38 [rsp]=0x702e2ec013 range=0x702e2e0000+0x10000 writable=true

   No `[l1-null]` line (a tagged entry for a nonzero RIP with no host code) appeared
   in any run. The guest really jumps to 0, and the return address it pushed is
   page offset `0xc013` of a Mono code region.
2. The guest bytes around that return address (`[low-rip]`, read when the RIP
   is refused) are Mono x86-64 code:

       bff0: lea rsp,[rsp]; nop; mov r11, 0x702e2e1010   (imm64 at 0x...bff8: patched)
       c000: lea rsp,[rsp]; nop; mov r11, 0               (imm64 at 0x...c008: 0)
       c010: call r11                                     <- the call to 0
       ...   mov r11, 0; call r11                         (imm64 at 0x...c028: 0)
       ...   mov r11, 0; call r11                         (imm64 at 0x...c048: 0)
       c068: e8 rel32; 08; .quad arg                      (a Mono specific trampoline)

   Mono emits `mov r11, 0` as a template and patches each one with its target before
   it publishes the method. Each template in page `0x...c000` is unpatched, while those
   in the previous page (`0x...bfb0`, `0x...bfd0`, `0x...bff8`) hold real targets.
3. A compile log after each SMC fault (`[post-smc]`) gives the order. Mono creates
   the trampoline at `0x...c068`. Its call displacement is the `[smc-byte] ml1018 #3` byte
   store at `0x...c069`. Then FEX compiles the method's entry `0x...bf70`, called from
   its caller at `0x...becb`, once Mono has published it. The method calls the trampoline
   (`compile rip=0x...c068 [rsp]=0x...bfbb`), its first call site is backpatched through
   `MonoBackpatcherWrite` (`[mbw] addr=0x...bfb0`, read back correct through the
   guest address), and the next template in page `0x...c000` calls 0.
4. Dumps of page `0x...c000` before and after FEX handles the `0x...c069` fault
   (`[smc-dump]`: page invalidation and `NtProtectVirtualMemory`) are identical. The
   templates are already 0 when the fault arrives. FEX's handling does not change the
   page.
5. Snapshots of the page at each later compile show two more writes to it after
   the fault: byte `0x...c017` changes from `0x48` to `0x50` (`mov rcx, rax` becomes
   `push rax; mov ecx, eax`), and Mono's `UNWIND_INFO` (`01 04 02 05 04 03 01 50`, for
   `push rbp; mov rbp, rsp`) is written at `0x...c058`, over 8 bytes of the method's code
   16 bytes before its end. That fits a second, shorter copy of the method's code over the
   first (Mono writes unwind data at the aligned end of the code), and a fresh copy has
   unpatched templates. No write to the three templates appears.
6. With `env.MADEIRA_ANON_EXEC_POOL=1` (Wine's RX pool view, every store emulated)
   the run ends the same way, at a later template (`[rsp]=0x...c053`; the imm64s at
   `0x...c028` and `0x...c048` are 0). So the
   cause is not the RW plain view (madeira-unix 0019/0027).
7. The same diagnostic build with `env.MADEIRA_KEEP_SMC=1` reaches first frame.

## Not known

- Who leaves those templates at 0: Mono patching them with 0, a second copy of
  the method over the patched one, or stores into the page lost between Mono and the
  guest view. The evidence above (the `0x...c017` byte and the unwind data at
  `0x...c058`) points at a second copy of the code. Why that happens only with tracking
  off is not known.
- Whether upstream FEX on Windows on Arm shows the same failure. The MonoHacks paths
  are the same here (`MonoBackpatcherWrite` for the call backpatcher, the frontend
  stopping at calls in writable code).

## Next step

Find what writes page `0x...c000` between Mono's `memcpy` of the method and its
publication: Mono's own stores (FEX's translation of `mono_codegen`'s copy and
patch loop), and Wine's store emulator for that page (`signal_arm64_ios.c`, the
store-emulation step: alias address, value, read back through the guest address). A
write watch on the page from the `memcpy` to the first call into the method shows
whether the second copy is Mono's or a replayed store. The diagnostic patches are kept
outside the repository (in the build area, beside the speedrun report) for that run.

## Runs

The game was installed from GOG (`.work/ui-runs/20261011T183939`) and uninstalled
afterwards (`.work/ui-runs/20261011T192110`). Each diagnostic IPA is a dev build of
`patches/fex` plus the diagnostic patches:

| Run (`.work/ui-runs/`) | IPA sha256 | Setting | Outcome |
| --- | --- | --- | --- |
| `20261011T184033` | `ebe5ee259b4410ccc9678d66900c496e11b7a2adcdf49952698bef52942e4586` | none | refused RIP 0, exit `0xf8f08f70` |
| `20261011T184617` | `1521b855bcffc3a73c9e8a4566224f847fff024c1205eb49d8e149297740793b` | none | `[low-rip]` call-site bytes |
| `20261011T185401` | `3af2ec0ec8eb02ff479a15270cd1ad7af34093191c6e947a49b4111e92d8963e` | none | `[post-smc]` compile order |
| `20261011T185954` | `603d46c235ef01bb93d258df444ee2c811cb25b7a03dc20f72aa90181f451f8e` | none | `[smc-dump]` page before/after |
| `20261011T190506` | `51e531f0005e907795bd08713b2ee48a818d436642b83b02f0748fb4bb9482f0` | none | page snapshots per compile |
| `20261011T190628` | `51e531f0005e907795bd08713b2ee48a818d436642b83b02f0748fb4bb9482f0` | `env.MADEIRA_KEEP_SMC=1` | first frame +10 s |
| `20261011T191424` | `c7acfb47ba63ffec49db3f331be0cf7e149b93d07d15ab7ac45ca841c7448539` | none | `[mbw]` backpatches in the page |
| `20261011T191620` | `c7acfb47ba63ffec49db3f331be0cf7e149b93d07d15ab7ac45ca841c7448539` | `env.MADEIRA_ANON_EXEC_POOL=1` | same crash, next template |

The phone was left on the unmodified build of this branch's HEAD,
`4c7431519e09b8a654ea1c9c34ffc0d00c016cb3de81636132e9d2c99f5ffe9a`.

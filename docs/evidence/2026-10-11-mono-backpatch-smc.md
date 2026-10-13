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

## Second attempt (2026-10-11, later)

Still no fix; the compatibility default stays. Three more diagnostic builds (the earlier
FEX patches plus a store ring in Wine's Mach store emulator and a FEX record of the guest
bytes each block in writable code was compiled from; none committed) narrow it down:

1. **No store is lost.** With tracking off, Wine forces the whole Mono code chunk read-only
   when FEX disables tracking (the `ml638` re-protect after FEX's `NtProtectVirtualMemory`),
   so the first stores to page `0x...c000` reach the Mach store emulator. The ring shows
   every store into the method's tail, in order. Each one read back the same through the
   guest address and the RW alias:

       E addr=0x...c000..c040  STP Q (memcpy of the method, templates 0)
       E addr=0x...c060..c067  STRB  (memcpy tail, ends with ret at 0x...c067)
       E addr=0x...c068..c06e  the specific trampoline (call, 08, .quad arg)
       U addr=0x...c069        STLURB, delivered to FEX ([smc-byte]), retried as STURB
       E addr=0x...c017        STURB of 0x50   (pc of Mono's one-byte code store)
       E addr=0x...c058/c05c   two STUR W: UNWIND_INFO 01 04 02 05 04 03 01 50

   Nothing ever writes the three `mov r11` imm64s at `0x...c008`, `0x...c028` and
   `0x...c048`. The next method, at `0x...c080`, has its templates patched by the same
   Mono store (`STUR X, [x, #1]`) at `0x...c098` and `0x...c0c8`. With
   `env.MADEIRA_ANON_EXEC_POOL=1` (every store emulated) the ring shows the same sequence.
2. **Mono patches the wrong place.** The byte at `0x...c017` matches what Mono's `x86_patch`
   writes for a short branch at `0x...c016` with target `0x...c068`, the new trampoline:
   `0x50 = 0x...c068 - 0x...c017 - 1`. In memory, `0x...c015` holds `45 e0` (part of
   `mov [rbp-0x20], rax`), which `amd64_patch` reads as a REX prefix and then `loop`. The
   unwind data goes to method start `+0xe8`, while the copied code is `0xf8` bytes long.
   The same trampoline is the patched target of the call template at `0x...bfb0` (the
   compile log shows the call into it from `0x...bfbb`). So Mono's patch list and code
   length for this method do not match the bytes it copied. The call sites in the page
   before `0x...c000` match, and the later ones do not.
3. **No stale translation in the chunk.** At the refused RIP 0, 241 blocks had been
   compiled in the 64 KiB chunk. The log, capped at 60 entries, listed 60 blocks that had
   changed since: backpatched templates, and trampolines followed by new code. None of the
   listed blocks was still in the thread's lookup cache. All compiles in the window were on the one main thread (`2C`).

So the inference in "Not known" above was wrong: there is no second copy of the method and
no lost store. Mono, running single-threaded under FEX, finalises a method with metadata
that does not match the code it copied. This happens only with tracking off. The cause
must lie in how Mono's own native JIT code runs when its stores into code pages take a
different path: native stores, Wine's emulator, or FEX's SMC fault path with its in-place
`STLUR`/`STLURB` rewrites and call-return stack resets. That is not yet proven.

Next: compare the same method's compile with tracking kept (`MADEIRA_KEEP_SMC=1`, which
works), to find where Mono's emitted code or patch offsets first differ. Log, on thread
`2C`, each FEX SMC fault taken inside Mono's JIT (host pc, the guest return addresses)
and each call-return stack reset. The diagnostic patches are kept in the build area beside
the speedrun report (`pla115-diag2/`).

| Run (`.work/ui-runs/`) | IPA sha256 | Setting | Outcome |
| --- | --- | --- | --- |
| `20261011T194046` | `76f3f7a2327feb3b56b989d7f47db21d712567243e6a0680db4180d78e108aab` | | Duck Paradox installed from GOG |
| `20261011T194131` | same | `env.MADEIRA_KEEP_SMC=0` | refused RIP 0, exit `0xf8f08f70`; store ring |
| `20261011T194418` | same | `env.MADEIRA_KEEP_SMC=0 env.MADEIRA_ANON_EXEC_POOL=1` | same; full store ring |
| `20261011T195046` | `e35bb14424f265377ee12d120f34064ea7224b72a23642b334a0068f99f7401d` | `env.MADEIRA_KEEP_SMC=0` | same (exit `0xc0000409`); compiled-bytes record |
| `20261011T195715` | `cda0f7dd3f6db30fef16b357f91789a822f0efd294db823a977e5fd1d5c2cc96` | | branch HEAD build reinstalled; Duck Paradox uninstalled |

`env.MADEIRA_KEEP_SMC=0` as the player's own setting overrides the compatibility default,
so no build without the default was needed. Run `20261011T194321` passed both variables
comma-separated, which the runtime field does not split. It is the same as `194131`.


# Duck Paradox: an exit at start with FEX's SMC tracking off for Mono

**Date:** 2026-10-09. **Branch:** `dev/compat-speedrun`. **Linear:** PLA-52.
**Title:** Duck Paradox (GOG `1816031813`), x86-64, Unity (Mono `mono-2.0-bdwgc.dll`),
Direct3D 11, GOG Galaxy SDK.

**Result:** with FEX's self-modifying-code (SMC) tracking kept on, the game draws its
menu on Vulkan at 16:9 (first frame about 5.5 s after Play), and Play starts the story
intro. With the tracking off, as every Mono game runs since `patches/fex` 0003, it ends
1.5 s after start with exit code `0xf8f88f70`. A compatibility default (decision 0074)
now keeps the tracking on for this title: `runtime` `env.MADEIRA_KEEP_SMC=1`.

## Before

IPA `05707e9e1762f5764e9f6ad084927b39e7c12686b346e61934bd490258ed4db7`, Vulkan, 16:9
(`.work/ui-runs/20261009T024354`; the 2026-10-07 run with IPA 3603b4da ended the same).
The exit code is a corrupt exception record, not the fault. The fault is in Mono's
JIT-compiled code, just after a byte store into another of its code pages:

    E 2C [smc-off] Mono backpatcher detected: SMC detection disabled, as upstream
    ...
    D 2C Handled self-modifying code: pc: 127BC7B94 fault: 702A1CC069
    E 2C [smc-byte] ml1018 #3 DECLINED backpatch: insn 1900037A at pc 127BC7B94 is a BYTE release store (fault 702A1CC069); ...
    002c:err:seh:segv_handler SEGV at pc=0x0 addr=0x0 esr=0x82000006 ...
    002c:err:seh:segv_handler   [guest-state] x28=0x7c06001140 rip=0x702a1cbfdb
    002c:err:seh:segv_handler   branch@(lr-4)=d63f0160
    002c:err:seh:call_vectored_handlers [veh-corrupt] #1 handler 00000073F9043CB0 ret=0 WRECKED the context: Rip 0000000000000000 -> 0000007C06001000, ...
    002c:err:seh:NtRaiseException Unhandled exception code f8f88f70 flags 73 addr 0x1400211200000000

A translated block of the guest code at `0x702a1cbfdb` (16 KiB page `0x702a1c8000`,
the block running into the next page, the one just written) branched (`blr x11`) to a
null host address. Mono's vectored handler (`mono-2.0-bdwgc.dll+0x303cb0`) then
corrupted the context, and the process ended with the record's garbage.

## After

`runtime` `env.MADEIRA_KEEP_SMC=1` (`patches/fex` 0003's A/B switch), as a game-page
setting on the same IPA (`.work/ui-runs/20261009T024612`), then as the shipped
compatibility default with no runtime setting of the player's, IPA
`1335554d72ba43cb11d51bdaeee74d2d168eaaab3c5c864592d86c2ffb80d218`
(`.work/ui-runs/20261009T025146`, `.work/ui-runs/20261009T025306`):

    title: config: environment MADEIRA_KEEP_SMC=1
    E 2C [smc-off] Mono backpatcher detected: SMC detection KEPT (MADEIRA_KEEP_SMC=1)

No `SEGV`; the menu (title, Play, Options, Quit, animated ducks) at first frame +30 s,
and A on Play shows the first story card. Tracking costs start-up time in Mono games
(Hollow Knight: [first-run stutter](2026-09-26-first-run-stutter.md)), so it stays
per title.

## Not done

Why a block that crosses into a page FEX invalidated branches to a null target with the
tracking off (a Madeira/iOS FEX code-cache path, not seen in other Mono games so far);
the compatibility default stands until that is fixed.

## Later

2026-10-11 ([mono-backpatch-smc](2026-10-11-mono-backpatch-smc.md)): the null target is
not a FEX link. The guest itself calls 0 from a Mono method whose `mov r11` call
templates in one page are still unpatched when it runs, so FEX translates what is in
memory. The cause is not found yet; the compatibility default stays.

# Evidence: builtin window classes registered in every pseudo-process again

**Date:** 2026-10-09. **Linear:** PLA-113 (House of Golf 2's prerequisite installer), PLA-67.
**Status:** host-only. Built (dev IPA sha256
`f0c90391d624d0fdaf395e8605a3f0c842aa1e161fc5edd6e39439da6e3e94fa`), **not yet checked on the
phone** (the phone was blocked by an iOS system alert that needs the owner).

## The failure

House of Golf 2's `UEPrereqSetup_x64.exe` (a WiX Burn bootstrapper, i386) stopped before
showing anything (run `20261009T001119`, thread `0038`):

```
0038:err:wow:wow64_NtUserCreateWindowEx Playport: i386 NtUserCreateWindowEx( class "Static", name "", style 4800000d, ex_style 00000000, parent 000000000001003A, instance 0000000000000000 ) failed, error 6
0038:err:wow:wow64_NtUserCreateWindowEx Playport: i386 NtUserCreateWindowEx( class "WixStdBA", name "UE Prerequisites (x64) Setup", style 000a0000, ex_style 00000000, parent 0000000000000000, instance 000000007A220000 ) failed, error 6
```

The `WixStdBA` window was created (`[win-pos]` and `[win-text]` lines for `0x1003a`). Its
`WM_CREATE` creates the theme's controls. The first one, an owner-drawn `Static` image, failed,
so `WM_CREATE` failed and the main window failed with the child's error.

## The cause

Error 6 is `STATUS_INVALID_HANDLE` from wineserver's `grab_class`: `create_window` found no
`Static` class in the title's process. wineserver keeps window classes per process. On iOS every
pseudo-process shares one win32u unix image, so `register_builtin_classes`
(`build/win32u-unix/class_ios.c`) calls `register_builtins` once per (pid, PEB), and
`NtUserInitializeClientPfnArrays` registers the desktop and message classes for every later
process (madeira-unix 0043). But the Wine 11.1 port (madeira-unix 0020) brought Wine's
`if (descr->atom) return; /* already registered */` into `register_builtin`, and
`builtin_classes` is one table for the whole session. Once the session root had set the atoms,
every later process registered nothing. The `[builtin-classes] registered builtin classes for
pid=0028` line was logged anyway.

The same error is in earlier phone logs. Every Portal 2 run has one (its message-only window):
`class "Message", ..., parent FFFFFFFFFFFFFFFD ... failed, error 6`. The Shogun Showdown Inno
Setup installer logged `class "#32770" ... failed, error 6`. Only i386 titles log it (the message
comes from the wow64win thunk), but x86-64 titles hit the same server check.

## The fix

madeira-unix 0105 (`win32u: register the builtin classes in every pseudo-process again`):

- `register_builtin` always registers the class for the calling process and keeps only the atom
  in the shared table. The user atom table is global, so every process gets the same atom.
- The class takes the calling process's user32 (`ios_cur_user32_module`), not the session
  root's.
- Edit's extra bytes (6 under WoW64) are chosen for each registration. Before, a WoW64 process
  wrote 6 into the shared table, and a later x86-64 process would have got 6 too.
- `register_builtins` leaves out the desktop and message classes:
  `register_desktop_class` already registers them for every process.

## Phone check still owed

Every title after the session root now registers the builtin classes, so its own `Static`,
`Button`, `Edit`, `#32770` and `Message` windows are created. win32u's default IME window, which
is upstream behaviour, is now created for each title's first top-level window too. Before the
fix that creation failed quietly. Check that nothing new stops: Hollow Knight (x86-64), Portal 2
(i386, now gets its message window) and Super Meat Boy (i386). Then run House of Golf 2's
`UEPrereqSetup_x64.exe` from a Local import.

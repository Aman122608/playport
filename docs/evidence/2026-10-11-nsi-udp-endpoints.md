# iOS NSI UDP endpoint table: Cat Mail Co. Demo (PLA-112, PLA-27)

**Date:** 2026-10-11. **Phone:** iPhone18,4, iOS 27.0, dev IPAs upgraded in place.
**Title:** Cat Mail Co. Demo (Steam 4622530): Unity 6 IL2CPP, x86-64, Direct3D 11,
URP, Steamworks/Mirror multiplayer.

## Finding

On dev IPA `ed58bb43d995a599fc321cbb41a4ac956d0ff4d19bc7deee9d5c2c6d243b1759`
the main menu reaches its first frame after about 4 s on both backends, but choosing
Singleplayer, then the tutorial save, ends the game during the loading screen with
`0xC0000005` (the app's "stopped unexpectedly" box). The main thread reads address 0
in `GameAssembly.dll` immediately after the in-process NSI fallback's first call:

    002c:err:nsi:NsiEnumerateObjectsAllParametersEx no \\.\Nsi (err 2); serviced in-process, status 0xc00000bb rev=ml472
    wine: Unhandled page fault on read access to 0000000000000000 at address 00000073F5A1E976 (thread 002c), starting debugger...

The launch setting `runtime: env.WINEDEBUG=+iphlpapi,+nsi` names the call:
`GetExtendedUdpTable(family 2, UDP_TABLE_BASIC)`, which is .NET's
`IPGlobalProperties.GetActiveUdpListeners()` (`Win32IPGlobalProperties` is in the
game's IL2CPP metadata). The fallback served TCP, NDIS and IP tables but not UDP, so
the call failed with `ERROR_FILE_NOT_FOUND` and the game dereferenced the missing table.

## Change

Madeira-unix 0111 compiles Wine's `nsiproxy.sys/udp.c` into the unix archive beside
NDIS and IP and adds its module to the fallback's table lookup. Its endpoint table
comes from the in-process wineserver (`get_udp_endpoints`); its statistics table needs
`netinet/udp_var.h`, which the iOS SDK lacks, and still returns `STATUS_NOT_SUPPORTED`.

## Result

On dev IPA `12ecf6300fe9005b89be0a9ffca15834d6f281b03b03e0cb8721de0547d8d87a` the
same route logs `[nsi-ios] module=eb004a02 table=1 status=00000000 count=1` four
times, loads (about 50 s on the loading screen) and reaches the tutorial's
first-person scene, which answers the pad. The 3D scene draws washed out and
incomplete on Vulkan (DXVK), with or without the game's 4x MSAA, and correctly on
DXMT, so the title's compatibility default is DXMT. On dev IPA
`8948002db2be71253cfe38a75963b479d3859d4f62fdb1584b20d140e6c22138` (with that
default) a play at 16:9 with no backend chosen logs `graphics=dxmt`, loads the
tutorial and shows its warehouse scene correctly after moving and turning, more
than a minute into play. Screenshots stay in the run directories under `.work`.

#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The compatibility probes: small Windows programs that check one runtime path
# on the phone, built with llvm-mingw into $PLAYPORT_BUILD/compat/probes/<name>/,
# a folder to put in the dev app's Documents/Import and add with
# `pp ui --action import:Import/<name>` (docs/plans/2026-10-11-opengl-zink.md).
#
#   gl21   x86-64 OpenGL 2.1 through WGL: the pointer-path checks, clear, a
#          fixed-function and a GLSL 1.20 triangle, SwapBuffers; `stress`
#          measures 20,000 vertices a frame (gl21.c)
#   gl21-i386  the same for i386 (WoW64): gl21-i386.exe
#   wgl    x86-64 WGL behaviours games rely on: a dummy window, pixel formats
#          (depth/stencil, sRGB, MSAA), pbuffers, shared contexts, swap
#          interval, resize, a mode change, read back where it can (wgl.c)
#
#   tools/compat/probes/build.sh
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/../../../build/lib.sh"
CC=${LLVM_MINGW:?set LLVM_MINGW (pp setup)}/bin/x86_64-w64-mingw32-clang
CC32=$LLVM_MINGW/bin/i686-w64-mingw32-clang
OUT=$PLAYPORT_BUILD/compat/probes

mkdir -p "$OUT/gl21" "$OUT/gl21-i386"
"$CC" -O2 -Wall -Wextra -Werror -Wno-unused-parameter -mwindows -Wl,--no-insert-timestamp \
    -o "$OUT/gl21/gl21.exe" "$HERE/gl21.c" -lopengl32 -lgdi32 -luser32
"$CC32" -O2 -Wall -Wextra -Werror -Wno-unused-parameter -mwindows -Wl,--no-insert-timestamp \
    -o "$OUT/gl21-i386/gl21-i386.exe" "$HERE/gl21.c" -lopengl32 -lgdi32 -luser32
mkdir -p "$OUT/wgl"
"$CC" -O2 -Wall -Wextra -Werror -Wno-unused-parameter -mwindows -Wl,--no-insert-timestamp \
    -o "$OUT/wgl/wgl.exe" "$HERE/wgl.c" -lopengl32 -lgdi32 -luser32
for probe in gl21 gl21-i386 wgl; do
    echo "$(sha256sum "$OUT/$probe/$probe.exe" | cut -c1-16)  $probe.exe ($(stat -c %s "$OUT/$probe/$probe.exe") bytes) in $OUT/$probe"
done

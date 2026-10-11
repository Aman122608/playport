#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The compatibility probes: small Windows programs that check one runtime path
# on the phone, built with llvm-mingw into $PLAYPORT_BUILD/compat/probes/<name>/,
# a folder to put in the dev app's Documents/Import and add with
# `pp ui --action import:Import/<name>` (docs/plans/2026-10-11-opengl-zink.md).
#
#   gl21   x86-64 OpenGL 2.1 through WGL: clear, a fixed-function and a GLSL 1.20
#          triangle, SwapBuffers (gl21.c)
#
#   tools/compat/probes/build.sh
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/../../../build/lib.sh"
CC=${LLVM_MINGW:?set LLVM_MINGW (pp setup)}/bin/x86_64-w64-mingw32-clang
OUT=$PLAYPORT_BUILD/compat/probes

mkdir -p "$OUT/gl21"
"$CC" -O2 -Wall -Wextra -Werror -Wno-unused-parameter -mwindows -Wl,--no-insert-timestamp \
    -o "$OUT/gl21/gl21.exe" "$HERE/gl21.c" -lopengl32 -lgdi32 -luser32
echo "$(sha256sum "$OUT/gl21/gl21.exe" | cut -c1-16)  gl21.exe ($(stat -c %s "$OUT/gl21/gl21.exe") bytes) in $OUT/gl21"

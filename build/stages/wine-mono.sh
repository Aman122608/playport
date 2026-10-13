#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fetches Wine Mono, the .NET runtime Wine's mscoree loads (decision 0077), as
# Proton ships it: the release pins.lock's wine-mono row names, the version the
# wine pin's mscoree expects (dlls/mscoree/mscoree_private.h WINE_MONO_VERSION),
# in its shared-directory form (wine-mono-<version>-x86.tar.xz), checked against
# its sha256 below. Its source release, the Corresponding Source and where the
# notices are collected from (build/notices-wine-mono.py), is fetched beside it.
#
#   build/stages/wine-mono.sh
#
# Both tarballs are cached in $PLAYPORT_BUILD/cache/wine-mono-<version>/, and the
# binary one is unpacked there to wine-mono-<version>/, which
# build/stages/stage-artifacts.py records and stages, unmodified, as
# Runtime/mono/wine-mono-<version>/: mscoree finds it under WINEDATADIR (the app
# root) as \mono\wine-mono-<version>, the layout of Proton's share/wine/mono.
# The tarball's symbolic links (the framework directories' links into the GAC)
# are staged as copies of their targets: an IPA holds no links.
set -euo pipefail
. "$(dirname "$0")/../lib.sh"

SHA256=54a1b0111c3fe4b785eae688af94d27e64995707ad648b6fee8000381b80d298
SRC_SHA256=9f4f03285fbeed484980efbb1eba1c19069244aea401d2ff1585e3990782edf7
version=$(pin wine-mono)
url=$(pin_url wine-mono)/$version
cache=$PLAYPORT_BUILD/cache/wine-mono-$version
tarball=$cache/wine-mono-$version-x86.tar.xz
src=$cache/wine-mono-$version-src.tar.xz
tree=$cache/wine-mono-$version

# Written to a private temporary beside its final path, then moved into place: two
# builds (a pp sync candidate's and this checkout's) may share this cache.
tmps=()
trap 'rm -rf "${tmps[@]}"' EXIT
mkdir -p "$cache"
fetch() {   # fetch FILE SHA256
    echo "$2  $1" | sha256sum -c --status 2>/dev/null && return
    local part
    part=$(mktemp "$cache/part.XXXXXX"); tmps+=("$part")
    curl -fsSL -o "$part" "$url/$(basename "$1")"
    echo "$2  $part" | sha256sum -c --status || { echo "stage-wine-mono: $url/$(basename "$1") is not sha256 $2" >&2; exit 1; }
    mv "$part" "$1"
}
fetch "$tarball" "$SHA256"
fetch "$src" "$SRC_SHA256"

# The unpacked tree is complete when its stamp names the tarball's sha256.
if [ "$(cat "$tree/.unpacked-from" 2>/dev/null)" != "$SHA256" ]; then
    unpack=$(mktemp -d "$cache/unpack.XXXXXX"); tmps+=("$unpack")
    tar -xJf "$tarball" -C "$unpack"
    [ -d "$unpack/wine-mono-$version/bin" ] || { echo "stage-wine-mono: $tarball has no wine-mono-$version/bin" >&2; exit 1; }
    echo "$SHA256" > "$unpack/wine-mono-$version/.unpacked-from"
    old=$(mktemp -d "$cache/old.XXXXXX"); tmps+=("$old")
    [ -d "$tree" ] && mv "$tree" "$old/"
    mv "$unpack/wine-mono-$version" "$tree"
fi
echo "wine-mono $version: $(find "$tree" -type f ! -name .unpacked-from | wc -l) files, $(find "$tree" -type l | wc -l) links, $(du -sm "$tree" | cut -f1) MB"

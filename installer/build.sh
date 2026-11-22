#!/bin/sh
# ASCENT: build the setup program (docs/install.md).
#
#   installer/build.sh [CONTENT_DIR]
#
# 1. the release build (CMake preset "release", folder build-rel)
# 2. "cmake --install" into build-rel/stage
# 3. ISCC with installer/ascent.iss: build-rel/setup/ascent-dtu-setup-<version>.exe
#
# CONTENT_DIR (default gamedata/content) is the folder of the content packs
# (tools/content_pack.py). Its manifest.txt gives the download sizes that
# the setup program shows. The server address is in installer/content.ini.
#
# Needs MSYS2 UCRT64 (gcc, cmake, ninja, SDL3) and Inno Setup 6.5 or later.
set -e
cd "$(dirname "$0")/.."
export PATH=/c/msys64/ucrt64/bin:$PATH
VERSION=${ASCENT_VERSION:-1.0}
CONTENT=${1:-gamedata/content}
ISCC=${ISCC:-"$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe"}
[ -x "$ISCC" ] || ISCC="/c/Program Files (x86)/Inno Setup 6/ISCC.exe"

cmake --preset release -DASCENT_VERSION="$VERSION"
cmake --build --preset release
rm -rf build-rel/stage
cmake --install build-rel --prefix build-rel/stage

# The sizes in MB of the content choices, from the manifest
TEX_MB=520
MOV_MB=490
if [ -f "$CONTENT/manifest.txt" ]; then
  TEX_MB=$(awk -F'\t' '$1=="textures"||$1=="ui" {s+=$5} END {printf "%d", s/1048576+0.5}' "$CONTENT/manifest.txt")
  MOV_MB=$(awk -F'\t' '$1=="movies" {s+=$5} END {printf "%d", s/1048576+0.5}' "$CONTENT/manifest.txt")
fi

# MSYS must not change the /D options into paths
MSYS2_ARG_CONV_EXCL="*" "$ISCC" /Q "/DAPPVERSION=$VERSION" "/DTEXTURES_MB=$TEX_MB" "/DMOVIES_MB=$MOV_MB" \
  "/DSTAGE=..\build-rel\stage" "/DOUTDIR=..\build-rel\setup" installer/ascent.iss
ls -l build-rel/setup

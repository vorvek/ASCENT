#!/bin/sh
# ASCENT: build the Linux release tarball (docs/install.md).
#
#   installer/build-linux.sh
#
# 1. the release build (Release, ASCENT_RELEASE=ON) in build-rel-linux
#    (or ASCENT_BUILD_DIR), with SDL3 built from source and linked
#    statically: the program needs only the C library of the system. SDL3
#    loads X11, Wayland, ALSA, PulseAudio and PipeWire when it starts.
# 2. the staging folder: the game, ascent-import, content.ini, setup.sh,
#    README.txt and the license texts. No game file and no original art.
# 3. build-rel-linux/ascent-dtu-<version>-linux-x86_64.tar.gz
#
# The variables: ASCENT_VERSION (default 1.0), ASCENT_BUILD_DIR, and
# SDL3_SOURCE_DIR (a checkout of SDL release-3.4.20, to build without the
# download). The build needs the packages of the Linux job of
# .github/workflows/build.yml.
set -eu
cd "$(dirname "$0")/.."
VERSION=${ASCENT_VERSION:-1.0}
BUILD=${ASCENT_BUILD_DIR:-build-rel-linux}
NAME=ascent-dtu-$VERSION-linux-$(uname -m)
STAGE=$BUILD/stage/$NAME

set -- -DCMAKE_BUILD_TYPE=Release -DASCENT_RELEASE=ON -DASCENT_VERSION="$VERSION" \
  -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON
if [ -n "${SDL3_SOURCE_DIR:-}" ]; then
  set -- "$@" -DFETCHCONTENT_SOURCE_DIR_SDL3="$SDL3_SOURCE_DIR"
fi
cmake -S . -B "$BUILD" -G Ninja "$@"
cmake --build "$BUILD" --target ascent ascent-import

# The game must not need a shared SDL3.
if ldd "$BUILD/engine/main/ascent" | grep -q SDL; then
  echo "build-linux.sh: ascent links a shared SDL3" >&2
  exit 1
fi

rm -rf "$BUILD/stage"
mkdir -p "$STAGE"
cp "$BUILD/engine/main/ascent" "$BUILD/installer/ascent-import" "$STAGE/"
strip "$STAGE/ascent-import"
cp installer/linux/setup.sh "$STAGE/"
# A Windows checkout has CRLF line ends in the text files: remove the CR.
tr -d '\r' < installer/content.ini > "$STAGE/content.ini"
sed "s/@VERSION@/$VERSION/g" installer/linux/README.txt | tr -d '\r' > "$STAGE/README.txt"
tr -d '\r' < engine/readme.txt > "$STAGE/LICENSE-PARALLAX.txt"
SDL_SRC=${SDL3_SOURCE_DIR:-$BUILD/_deps/sdl3-src}
tr -d '\r' < "$SDL_SRC/LICENSE.txt" > "$STAGE/LICENSE-SDL3.txt"
chmod 755 "$STAGE/setup.sh" "$STAGE/ascent" "$STAGE/ascent-import"
chmod 644 "$STAGE/content.ini" "$STAGE/README.txt" "$STAGE"/LICENSE-*.txt

tar -C "$BUILD/stage" --owner=0 --group=0 --numeric-owner -czf "$BUILD/$NAME.tar.gz" "$NAME"
ls -l "$BUILD/$NAME.tar.gz"

#!/bin/sh
# deps.sh [build|dosbox|capture]: the pinned tools the build and the tests need, built from source into
# build/deps/bin. build: jwasm and jwlink (make runs this). dosbox: the headless DOSBox-X the tests
# run on. capture: the same with the debug socket's video_start (tools/demo.py).
# DEPS_CACHE keeps the clones and builds across checkouts (default build/deps/src).
# jwasm and jwlink are C89: their own bool is a C23 keyword, so CC builds them as gnu17;
# gcc 14 also rejects jwlink's mismatched pointers.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/build/deps/bin"
CACHE="${DEPS_CACHE:-$ROOT/build/deps/src}"

JWASM=9fd1afd1a0d6fcebeba975f6586e33356e319e32
JWLINK=4dfdd7896c39b9567f4ea9bfbb7eda601277311e
DOSBOX=eea2b2da0203d37ff51e34a8f714af8110fe0162
DOSBOX_CAPTURE=f9e03d0574ac0a1157c3e84e850fdc37adc669b1

# clone REPO COMMIT DIR: the repository at COMMIT in DIR, once.
clone() {
    [ -d "$3/.git" ] && return 0
    git clone -q --filter=blob:none "https://github.com/ali-mosavian/$1.git" "$3.tmp"
    git -C "$3.tmp" checkout -q "$2"
    mv "$3.tmp" "$3"
}

# built DIR COMMAND...: COMMAND run in DIR unless DIR/.built says it ran to the end.
built() {
    dir=$1
    shift
    [ -f "$dir/.built" ] && return 0
    (cd "$dir" && unset DEBUG && "$@")
    touch "$dir/.built"
}

CC17="${CC:-cc} -std=gnu17 -Wno-error=incompatible-pointer-types"
mkdir -p "$BIN" "$CACHE"
case "${1:-build}" in
build)
    clone JWasm $JWASM "$CACHE/JWasm-$JWASM"
    built "$CACHE/JWasm-$JWASM" make -s -f GccUnix.mak CC="$CC17"
    clone JWlink $JWLINK "$CACHE/JWlink-$JWLINK"
    built "$CACHE/JWlink-$JWLINK" sh -c 'for part in dwarf/dw orl sdk/rc/wres .; do make -s -C $part -f GccUnix.mak CC="$1"; done' - "$CC17"
    cp "$CACHE/JWasm-$JWASM/GccUnixR/jwasm" "$CACHE/JWlink-$JWLINK/GccUnixR/jwlink" "$BIN/"
    ;;
dosbox | capture)
    commit=$DOSBOX
    [ "$1" = capture ] && commit=$DOSBOX_CAPTURE
    clone dosbox-x $commit "$CACHE/dosbox-x-$commit"
    built "$CACHE/dosbox-x-$commit" sh build-dosrun.sh
    name=dosbox-x
    [ "$1" = capture ] && name=dosbox-x-capture
    rm -f "$BIN/$name" # once a link to the source cache
    cp "$CACHE/dosbox-x-$commit/src/dosbox-x" "$BIN/$name"
    ;;
*)
    echo "deps.sh [build|dosbox|capture]" >&2
    exit 1
    ;;
esac

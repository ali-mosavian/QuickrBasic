#!/bin/sh
# deps.sh [build|dosbox|capture|sdl|shaders]: the pinned tools the build and the tests need, built from source
# into build/deps/bin. build: jwasm and jwlink (make runs this). dosbox: the headless DOSBox-X the tests
# run on. capture: the same with the debug socket's video_start (tools/demo.py). sdl: SDL 3 for the window's
# app, static, into build/deps/sdl3: the Mac's universal (arm64 and x86-64, macOS 13 on); SDL_HEADLESS=1, with
# no window but the offscreen driver's (a Linux box with no X11 or Wayland to build against). shaders: glslang and
# spirv-cross, which make shaders runs (tools/shaders.py).
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
SDL=f00dc919ca35b9756316e86c4777392de442f9b2 # release-3.4.20
GLSLANG=a57276bf558f5cf94d3a9854ebdf5a2236849a5a # 16.0.0
SPIRV_CROSS=7affe74d77f93a622bb5002789d5332d32e512ee # vulkan-sdk-1.4.328.1

# clone REPO COMMIT DIR [OWNER]: the repository at COMMIT in DIR, once.
clone() {
    [ -d "$3/.git" ] && return 0
    git clone -q --filter=blob:none "https://github.com/${4:-ali-mosavian}/$1.git" "$3.tmp"
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
sdl)
    clone SDL $SDL "$CACHE/SDL-$SDL" libsdl-org
    built "$CACHE/SDL-$SDL" sh -c 'cmake -S . -B out -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF \
        -DSDL_UNIX_CONSOLE_BUILD="$2" -DCMAKE_INSTALL_PREFIX="$1" >/dev/null && cmake --build out -j4 >/dev/null && cmake --install out >/dev/null' \
        - "$ROOT/build/deps/sdl3" "$([ -n "${SDL_HEADLESS:-}" ] && echo ON || echo OFF)"
    ;;
shaders)
    clone glslang $GLSLANG "$CACHE/glslang-$GLSLANG" KhronosGroup
    built "$CACHE/glslang-$GLSLANG" sh -c 'cmake -S . -B out -DCMAKE_BUILD_TYPE=Release -DENABLE_OPT=OFF \
        -DGLSLANG_TESTS=OFF -DENABLE_HLSL=OFF >/dev/null && cmake --build out -j4 --target glslang-standalone >/dev/null'
    clone SPIRV-Cross $SPIRV_CROSS "$CACHE/SPIRV-Cross-$SPIRV_CROSS" KhronosGroup
    built "$CACHE/SPIRV-Cross-$SPIRV_CROSS" sh -c 'cmake -S . -B out -DCMAKE_BUILD_TYPE=Release \
        -DSPIRV_CROSS_ENABLE_TESTS=OFF -DSPIRV_CROSS_SHARED=OFF >/dev/null && cmake --build out -j4 --target spirv-cross >/dev/null'
    cp "$CACHE/glslang-$GLSLANG/out/StandAlone/glslang" "$CACHE/SPIRV-Cross-$SPIRV_CROSS/out/spirv-cross" "$BIN/"
    ;;
*)
    echo "deps.sh [build|dosbox|capture|sdl|shaders]" >&2
    exit 1
    ;;
esac

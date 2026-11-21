#!/bin/sh
# The native-extension prototype (docs/native-extensions-design.md): one extension, ext.c, built
# as each platform's slice. Runs the Mac's (both halves) and the browser's (in node); prints what
# the DOS and Linux slices import, as they cannot run here.
set -e
cd "$(dirname "$0")"
LLVM=${LLVM:-/opt/homebrew/opt/llvm/bin}
LLD=${LLD:-/opt/homebrew/opt/lld/bin}
O=../../build/extproto
mkdir -p $O
WASM="--target=wasm32-unknown-unknown -ffreestanding -matomics -mbulk-memory -O2"
FEATURES=--features=atomics,bulk-memory,bulk-memory-opt,mutable-globals,sign-ext,call-indirect-overlong,nontrapping-fptoint,reference-types,multivalue

echo "== macOS: a universal dylib, dlopen"
MAC="-arch arm64 -arch x86_64 -mmacosx-version-min=13.0"
clang $MAC -dynamiclib -ffreestanding -fvisibility=hidden -O2 -o $O/ext.dylib ext.c
clang $MAC -O1 -o $O/host host.c
$O/host $O/ext.dylib
arch -x86_64 $O/host $O/ext.dylib
echo "symbols imported:"
nm -u -arch arm64 $O/ext.dylib
nm -u -arch x86_64 $O/ext.dylib

echo "== wasm: a side module in the host's memory and table, then a second thread"
$LLVM/clang $WASM -fPIC -fvisibility=hidden -c ext.c -o $O/ext.o
$LLD/wasm-ld -shared --experimental-pic --shared-memory $FEATURES -o $O/ext.wasm $O/ext.o 2>/dev/null
$LLVM/clang $WASM -std=c23 -c hostw.c -o $O/hostw.o
$LLD/wasm-ld --no-entry --import-memory --shared-memory $FEATURES --max-memory=16777216 --export-table \
    --growable-table --export=__stack_pointer --allow-undefined -o $O/hostw.wasm $O/hostw.o
node load.mjs $O/hostw.wasm $O/ext.wasm

echo "== DOS: an i386 DLL, -mregparm=3 as the core"
$LLVM/clang --target=i386-pc-win32 -march=i386 -mregparm=3 -ffreestanding -fno-asynchronous-unwind-tables -O2 \
    -c ext.c -o $O/ext.obj
$LLD/lld-link /dll /noentry /nodefaultlib /export:qx_init_ext /out:$O/ext.dll $O/ext.obj
$LLVM/llvm-readobj --coff-imports --coff-basereloc $O/ext.dll | grep -E "Import|Type:" | sort | uniq -c

echo "== Linux x86-64: an .so"
$LLVM/clang --target=x86_64-linux-gnu -ffreestanding -fPIC -fvisibility=hidden -O2 -c ext.c -o $O/ext.lnx.o
$LLD/ld.lld -shared -nostdlib -o $O/ext.so $O/ext.lnx.o
$LLVM/llvm-readobj --needed-libs --dyn-syms $O/ext.so | grep -E "NeededLibraries|Name:"

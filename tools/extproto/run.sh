#!/bin/sh
# The native-extension prototype (docs/native-extensions-design.md): ext.c, plain C functions,
# glued to ext.bas by tools/qbx.py, built as each platform's slice. Runs the Mac's (both halves)
# and the browser's (in node); prints what the DOS and Linux slices import, as they cannot run here.
set -e
cd "$(dirname "$0")"
LLVM=${LLVM:-/opt/homebrew/opt/llvm/bin}
LLD=${LLD:-/opt/homebrew/opt/lld/bin}
O=../../build/extproto
mkdir -p $O
python3 ../qbx.py glue ext.bas ext $O
EXT="ext.c $O/ext_qx.c"
C="-std=c23 -O2 -I. -I$O"
WASM="--target=wasm32-unknown-unknown -ffreestanding -matomics -mbulk-memory $C"
FEATURES=--features=atomics,bulk-memory,bulk-memory-opt,mutable-globals,sign-ext,call-indirect-overlong,nontrapping-fptoint,reference-types,multivalue

echo "== macOS: a universal dylib, dlopen"
MAC="-arch arm64 -arch x86_64 -mmacosx-version-min=13.0 $C"
clang $MAC -dynamiclib -ffreestanding -fvisibility=hidden -o $O/ext.dylib $EXT
clang $MAC -o $O/host host.c drive.c
$O/host $O/ext.dylib
arch -x86_64 $O/host $O/ext.dylib
echo "symbols imported:"
nm -u -arch arm64 $O/ext.dylib
nm -u -arch x86_64 $O/ext.dylib

echo "== wasm: a side module in the host's memory and table, then a second thread"
for f in $EXT; do $LLVM/clang $WASM -fPIC -fvisibility=hidden -c $f -o $O/$(basename $f .c).wasm.o; done
$LLD/wasm-ld -shared --experimental-pic --shared-memory $FEATURES -o $O/ext.wasm $O/ext.wasm.o $O/ext_qx.wasm.o 2>/dev/null
$LLVM/clang $WASM -c hostw.c -o $O/hostw.o
$LLVM/clang $WASM -c drive.c -o $O/drive.o
$LLD/wasm-ld --no-entry --import-memory --shared-memory $FEATURES --max-memory=16777216 --export-table \
    --growable-table --export=__stack_pointer --allow-undefined -o $O/hostw.wasm $O/hostw.o $O/drive.o
node load.mjs $O/hostw.wasm $O/ext.wasm

echo "== DOS: an i386 DLL, -mregparm=3 as the core"
for f in $EXT; do
    $LLVM/clang --target=i386-pc-win32 -march=i386 -mregparm=3 -ffreestanding -fno-asynchronous-unwind-tables $C \
        -c $f -o $O/$(basename $f .c).obj
done
$LLD/lld-link /dll /noentry /nodefaultlib /out:$O/ext.dll $O/ext.obj $O/ext_qx.obj
$LLVM/llvm-readobj --coff-imports --coff-exports --coff-basereloc $O/ext.dll | grep -E "Import|Name:|Type:" | sort | uniq -c

echo "== Linux x86-64: an .so"
for f in $EXT; do
    $LLVM/clang --target=x86_64-linux-gnu -ffreestanding -fPIC -fvisibility=hidden $C -c $f -o $O/$(basename $f .c).lnx.o
done
$LLD/ld.lld -shared -nostdlib -o $O/ext.so $O/ext.lnx.o $O/ext_qx.lnx.o
$LLVM/llvm-readobj --needed-libs --dyn-syms $O/ext.so | grep -E "NeededLibraries|Name:"

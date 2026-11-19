#!/usr/bin/env bash
#
# mayhem/build.sh — build dust3d's fuzz harnesses and its functional self-test.
#
# dust3d ships only a qmake build of the Qt GUI (application/application.pro). Its core
# (dust3d/** + the vendored autoremesher/tri-tri code) is Qt-free, so this builds the core
# sources application.pro lists into a static library with mayhem/Makefile — once with the
# sanitizer flags (fuzzing) and once with normal flags (the test oracle) — and links:
#   fuzz_ds3            .ds3 container + snapshot XML (Ds3FileReader, loadSnapshotFromXmlString)
#   fuzz_mesh_generate  snapshot XML -> dust3d::MeshGenerator::generate()
#   fuzz_glb_reader     application/sources/glb_reader.cc (GLB import) against Qt6 Core/Gui
# each also as <name>-standalone (run-once reproducer), plus mayhem/build/dust3d_selftest.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"
mkdir -p "$B"

# The core source list, as application.pro has it at this commit (so the build time-travels).
SRCS="$(grep -oE '\.\./(dust3d|third_party)/[^ ]+\.(cc|cpp|c)\b' application/application.pro \
        | sed 's#^\.\./##' | sort -u | tr '\n' ' ')"
[ -n "$SRCS" ] || { echo "no core sources found in application/application.pro" >&2; exit 1; }

# 1) sanitized, instrumented core for the fuzzers (DEBUG_FLAGS last so -gdwarf-3 wins).
FUZZ_FLAGS="-O1 $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
make -f mayhem/Makefile -j"$MAYHEM_JOBS" SRCS="$SRCS" OBJDIR="$B/fuzz" \
    CC="$CC" CXX="$CXX" XCXXFLAGS="$FUZZ_FLAGS" XCFLAGS="$FUZZ_FLAGS" "$B/fuzz/libdust3d.a"

# 2) the project's normal-flag build for the test oracle.
TEST_FLAGS="-O2 -DNDEBUG $COVERAGE_FLAGS"
make -f mayhem/Makefile -j"$MAYHEM_JOBS" SRCS="$SRCS" OBJDIR="$B/test" \
    CC="$CC" CXX="$CXX" XCXXFLAGS="$TEST_FLAGS" XCFLAGS="$TEST_FLAGS" "$B/test/libdust3d.a"

INC="-D_USE_MATH_DEFINES -Ithird_party/autoremesher/tbbshim -I. -Ithird_party -Ithird_party/rapidxml-1.13 -Ithird_party/eigen"
$CXX -std=c++17 $INC $TEST_FLAGS mayhem/test/dust3d_selftest.cc "$B/test/libdust3d.a" -lpthread \
    -o "$B/dust3d_selftest"

# 3) harnesses: libFuzzer binary + standalone reproducer each.
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$B/lsan_off.o"

QT_CFLAGS="$(pkg-config --cflags Qt6Core Qt6Gui)"
QT_LIBS="$(pkg-config --libs Qt6Core Qt6Gui)"
$CXX -std=c++17 $INC -Iapplication/third_party/json $QT_CFLAGS -fPIC $FUZZ_FLAGS \
    -c application/sources/glb_reader.cc -o "$B/glb_reader.o"

link() { # name extra-objs extra-libs...
    local name="$1"; shift
    local cflags="$1"; shift
    $CXX -std=c++17 $INC $cflags $FUZZ_FLAGS -c "mayhem/harnesses/$name.cc" -o "$B/$name.o"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/$name.o" "$@" "$B/lsan_off.o" -lpthread -o "$B/$name"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$B/standalone_main.o" "$B/$name.o" "$@" "$B/lsan_off.o" -lpthread \
        -o "$B/$name-standalone"
}
link fuzz_ds3 "" "$B/fuzz/libdust3d.a"
link fuzz_mesh_generate "" "$B/fuzz/libdust3d.a"
link fuzz_glb_reader "-Iapplication/sources $QT_CFLAGS -fPIC" "$B/glb_reader.o" "$B/fuzz/libdust3d.a" $QT_LIBS

ls -l "$B"/fuzz_* "$B/dust3d_selftest"

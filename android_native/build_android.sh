#!/bin/bash
# Build the native side of SonRac for Android (Git Bash on Windows) and stage
# it into the app's jniLibs:
#   libmain.so (game), librexruntime.so, librexgpu-xenos.so, libSDL3.so, libc++_shared.so
#
# usage: android_native/build_android.sh [jobs]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NDK_VER="${NDK_VER:-29.0.14206865}"
ANDROID_NDK="${ANDROID_NDK:-$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/ndk/$NDK_VER}"
VS_CMAKE="/c/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake"
export PATH="$VS_CMAKE/CMake/bin:$VS_CMAKE/Ninja:$PATH"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
BUILD_DIR="$ROOT/out/android-arm64"
REXSDK_DIR="/c/Users/EduPedu/Desktop/LibertyRecompAndroid/glue/rexglue-sdk-main"
REX_OUT_DIR="$BUILD_DIR/rex-out"
JNILIBS="$ROOT/android/app/src/main/jniLibs/arm64-v8a"
NDK_BIN="$ANDROID_NDK/toolchains/llvm/prebuilt/windows-x86_64/bin"
NDK_SYSROOT_LIB="$ANDROID_NDK/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android"
JOBS="${1:-$(( $(nproc) > 3 ? $(nproc) - 2 : 1 ))}"

[ -d "$ANDROID_NDK" ] || { echo "!! no NDK at $ANDROID_NDK (install NDK $NDK_VER)"; exit 1; }

# Git symlinks are plain text files in a Windows checkout; turn them back into real files.
python "$(cygpath -m "$REXSDK_DIR/../../os/android/scripts/materialize_symlinks.py")"

# ARMv8.2: inline LSE atomics; native ELF TLS (API 29+). Both are hot on the guest thread.
CPU_FLAGS="-march=armv8.2-a -fno-emulated-tls"
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
  echo "== configuring $BUILD_TYPE in $BUILD_DIR"
  cmake -S "$ROOT/android_native" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$(cygpath -m "$ANDROID_NDK")/build/cmake/android.toolchain.cmake" \
    -DANDROID_NDK="$(cygpath -m "$ANDROID_NDK")" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-29 \
    -DANDROID_STL=c++_shared \
    -DCMAKE_C_FLAGS="$CPU_FLAGS" \
    -DCMAKE_CXX_FLAGS="$CPU_FLAGS" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DREXGLUE_ENABLE_TRACY=OFF
fi

echo "== building with $JOBS jobs"
cmake --build "$BUILD_DIR" --target SonRac -- -j"$JOBS"

echo "== staging libraries"
mkdir -p "$JNILIBS"
rm -f "$JNILIBS"/*.so
for so in "$(find "$BUILD_DIR" -name libmain.so | head -1)" \
          "$REX_OUT_DIR"/librexruntime.so "$REX_OUT_DIR"/librexgpu-xenos.so "$REX_OUT_DIR"/libSDL3.so; do
  [ -f "$so" ] || { echo "!! missing $so"; exit 1; }
  if [ "${STRIP:-0}" = "1" ]; then
    "$NDK_BIN/llvm-strip" --strip-unneeded -o "$JNILIBS/$(basename "$so")" "$so"
  else
    cp "$so" "$JNILIBS/"
  fi
done
cp "$NDK_SYSROOT_LIB/libc++_shared.so" "$JNILIBS/"

echo "== checks"
"$NDK_BIN/llvm-nm" -D --defined-only "$JNILIBS/libmain.so" | grep -E ' SDL_main$' >/dev/null || {
  echo "!! SDL_main is not exported from libmain.so"; exit 1; }
# Every DT_NEEDED entry must be staged here or be a system library.
for so in "$JNILIBS"/*.so; do
  for need in $("$NDK_BIN/llvm-readelf" -d "$so" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p'); do
    case "$need" in
      libc.so|libm.so|libdl.so|liblog.so|libandroid.so|libvulkan.so|libEGL.so|libGLESv2.so|libGLESv3.so|libOpenSLES.so|libaaudio.so|libz.so|libmediandk.so|libnativewindow.so|libjnigraphics.so) ;;
      *) [ -f "$JNILIBS/$need" ] || { echo "!! $(basename "$so") needs $need, which is not staged"; exit 1; } ;;
    esac
  done
done
ls -la "$JNILIBS"
echo "== done"

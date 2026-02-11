#!/bin/bash
# prebuild.sh — Build VCpp library and dependencies for the editor server
# Runs inside Docker during image build (Stage 1)
#
# Produces /prebuilt/cache/ with all PCMs, BMIs, objects, and libraries
# needed to compile user code at request time.

set -euo pipefail

LAM_ROOT="/src/lam"
BUILD_DEPS="/build-deps"
BUILD_VCPP="/build-vcpp"
INSTALL_DIR="/prebuilt/install"
CACHE_DIR="/prebuilt/cache"

echo "=== Stage 1: Building dependencies ==="

# clang-scan-deps symlink (required for C++23 module scanning)
EMSDK_LLVM_BIN="${EMSDK}/upstream/bin"
if [ ! -f "${EMSDK_LLVM_BIN}/clang-scan-deps" ]; then
  echo "Creating clang-scan-deps symlink..."
  ln -sf "$(which clang-scan-deps 2>/dev/null || echo "${EMSDK_LLVM_BIN}/../lib/clang-scan-deps")" \
    "${EMSDK_LLVM_BIN}/clang-scan-deps" || true
fi

# Build top-level dependencies (concepts, symbols, linearalgebra)
mkdir -p "${BUILD_DEPS}"
emcmake cmake -S "${LAM_ROOT}" -B "${BUILD_DEPS}" \
  -G Ninja \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_DEPS}"
cmake --install "${BUILD_DEPS}" --prefix "${INSTALL_DIR}"

echo "=== Stage 2: Building VCpp web example ==="

# Build the web example (produces all object files, PCMs, and libraries)
mkdir -p "${BUILD_VCPP}"
emcmake cmake -S "${LAM_ROOT}/vcpp/examples/web" -B "${BUILD_VCPP}" \
  -G Ninja \
  -DCMAKE_PREFIX_PATH="${INSTALL_DIR}" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_VCPP}"

echo "=== Stage 3: Collecting artifacts ==="

# Delegate to artifact collection script
"${LAM_ROOT}/vcpp/editor/prebuilt/collect_artifacts.sh" \
  "${BUILD_VCPP}" "${INSTALL_DIR}" "${CACHE_DIR}"

echo "=== Prebuild complete ==="
echo "Cache directory:"
find "${CACHE_DIR}" -type f | sort

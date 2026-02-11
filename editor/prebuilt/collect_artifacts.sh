#!/bin/bash
# collect_artifacts.sh — Collect build artifacts into the prebuilt cache layout
#
# Usage: collect_artifacts.sh <build_dir> <install_dir> <cache_dir>
#
# Produces:
#   cache/pcm/       — PCM files (vcpp partitions + primary + backend)
#   cache/bmi/       — BMI files (dependency modules)
#   cache/obj/       — Object files (.o)
#   cache/lib/       — Static libraries (.a)
#   cache/templates/ — Modmap, shell.html, runner.html, graph JS

set -euo pipefail

BUILD_DIR="${1:?Usage: collect_artifacts.sh <build_dir> <install_dir> <cache_dir>}"
INSTALL_DIR="${2:?}"
CACHE_DIR="${3:?}"

# The directory structure mirrors CMake's internal layout
VCPP_OBJ_DIR="${BUILD_DIR}/CMakeFiles/vcpp_web_test.dir"
CXX23_DIR="${BUILD_DIR}/CMakeFiles/__cmake_cxx23.dir"

# ===========================================================================
# Helper functions
# ===========================================================================

find_dep_dir() {
  find "${BUILD_DIR}/CMakeFiles" -maxdepth 1 -name "${1}*" -type d | head -1
}

generate_modmap() {
  local build_dir="$1"
  local cache_dir="$2"
  local modmap="${cache_dir}/templates/user.modmap"

  # Start with the basic flags
  cat > "${modmap}" << 'MODMAP_HEADER'
-x c++-module
MODMAP_HEADER

  # Read the original web_test.cppm.o.modmap and rewrite paths
  local orig_modmap="${build_dir}/CMakeFiles/vcpp_web_test.dir/web_test.cppm.o.modmap"

  if [ ! -f "${orig_modmap}" ]; then
    echo "ERROR: Original modmap not found at ${orig_modmap}"
    exit 1
  fi

  # Process each line, rewriting paths to use cache directory
  while IFS= read -r line; do
    case "$line" in
      -x*)
        # Already written in header
        ;;
      -fmodule-output=*)
        # Skip — user compilation generates its own PCM
        ;;
      -fmodule-file=std=*)
        echo "-fmodule-file=std=${cache_dir}/pcm/std.pcm" >> "${modmap}"
        ;;
      -fmodule-file=vcpp=*)
        echo "-fmodule-file=vcpp=${cache_dir}/pcm/vcpp.pcm" >> "${modmap}"
        ;;
      -fmodule-file=vcpp.wgpu.web=*)
        echo "-fmodule-file=vcpp.wgpu.web=${cache_dir}/pcm/vcpp.wgpu.web.pcm" >> "${modmap}"
        ;;
      -fmodule-file=lam.concepts=*)
        local bmi_file
        bmi_file=$(basename "${line##*=}")
        echo "-fmodule-file=lam.concepts=${cache_dir}/bmi/${bmi_file}" >> "${modmap}"
        ;;
      -fmodule-file=lam.*)
        # Generic handler for lam.* modules (symbols, linearalgebra partitions)
        local module_spec
        module_spec="${line%=*}"
        module_spec="${module_spec#-fmodule-file=}"
        local bmi_file
        bmi_file=$(basename "${line##*=}")
        echo "-fmodule-file=${module_spec}=${cache_dir}/bmi/${bmi_file}" >> "${modmap}"
        ;;
      -fmodule-file=vcpp:*)
        # VCpp partition PCMs
        local module_spec
        module_spec="${line%=*}"
        module_spec="${module_spec#-fmodule-file=}"
        local pcm_file
        pcm_file=$(basename "${line##*=}")
        echo "-fmodule-file=${module_spec}=${cache_dir}/pcm/${pcm_file}" >> "${modmap}"
        ;;
      *)
        # Pass through any other flags
        [ -n "$line" ] && echo "$line" >> "${modmap}"
        ;;
    esac
  done < "${orig_modmap}"

  echo "Generated user.modmap with $(wc -l < "${modmap}") lines"
}

# ===========================================================================
# Main collection logic
# ===========================================================================

SYMBOLS_DIR=$(find_dep_dir "lam__symbols@synth")
LINEARALGEBRA_DIR=$(find_dep_dir "lam__linearalgebra@synth")
CONCEPTS_DIR=$(find_dep_dir "lam_concepts__concepts@synth")

echo "Build dir: ${BUILD_DIR}"
echo "Symbols BMI dir: ${SYMBOLS_DIR}"
echo "LinearAlgebra BMI dir: ${LINEARALGEBRA_DIR}"
echo "Concepts BMI dir: ${CONCEPTS_DIR}"

# Create cache directories
mkdir -p "${CACHE_DIR}"/{pcm,bmi,obj,lib,templates}

# ---- PCMs ----
echo "Collecting PCMs..."
cp "${CXX23_DIR}/std.pcm" "${CACHE_DIR}/pcm/"
# All vcpp PCMs
for pcm in "${VCPP_OBJ_DIR}"/*.pcm; do
  [ -f "$pcm" ] && cp "$pcm" "${CACHE_DIR}/pcm/"
done

# ---- BMIs ----
echo "Collecting BMIs..."
if [ -n "${SYMBOLS_DIR}" ]; then
  for bmi in "${SYMBOLS_DIR}"/*.bmi; do
    [ -f "$bmi" ] && cp "$bmi" "${CACHE_DIR}/bmi/"
  done
fi
if [ -n "${LINEARALGEBRA_DIR}" ]; then
  for bmi in "${LINEARALGEBRA_DIR}"/*.bmi; do
    [ -f "$bmi" ] && cp "$bmi" "${CACHE_DIR}/bmi/"
  done
fi
if [ -n "${CONCEPTS_DIR}" ]; then
  for bmi in "${CONCEPTS_DIR}"/*.bmi; do
    [ -f "$bmi" ] && cp "$bmi" "${CACHE_DIR}/bmi/"
  done
fi

# ---- Object files ----
echo "Collecting object files..."
# main.cpp.o
cp "${VCPP_OBJ_DIR}/main.cpp.o" "${CACHE_DIR}/obj/"
# web_test.cppm.o is NOT copied (user provides their own)

# VCpp source objects (deep path structure in CMake build)
# These are under a nested directory matching the source path
find "${VCPP_OBJ_DIR}" -name "vcpp*.cppm.o" -type f | while read -r obj; do
  cp "$obj" "${CACHE_DIR}/obj/$(basename "$obj")"
done

# ---- Static libraries ----
echo "Collecting libraries..."
cp "${BUILD_DIR}/lib__cmake_cxx23.a" "${CACHE_DIR}/lib/"

# Installed dependency libraries
for lib in liblam_symbols.a liblam_linearalgebra.a liblam_concepts.a; do
  if [ -f "${INSTALL_DIR}/lib/${lib}" ]; then
    cp "${INSTALL_DIR}/lib/${lib}" "${CACHE_DIR}/lib/"
  fi
done

# ---- Generate user.modmap ----
echo "Generating user.modmap..."
generate_modmap "${BUILD_DIR}" "${CACHE_DIR}"

echo "Artifact collection complete."

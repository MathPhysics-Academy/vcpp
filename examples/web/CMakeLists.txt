cmake_minimum_required(VERSION 3.31.6 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 23)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0.3)
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0.0)
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31.6)
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
else()
  message(FATAL_ERROR "Version lower than 3.31.6 not supported")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(vcpp_web_example LANGUAGES CXX)

# Emscripten needs manual __CMAKE::CXX23 target creation since CMake's
# detection doesn't find the std modules in Emscripten's toolchain
if(EMSCRIPTEN AND NOT TARGET "__CMAKE::CXX23")
  set(_emscripten_libcxx_path "/usr/local/Cellar/emscripten/4.0.23/libexec/llvm/share/libc++/v1")
  if(EXISTS "${_emscripten_libcxx_path}/std.cppm")
    add_library(__cmake_cxx23 STATIC)
    target_sources(__cmake_cxx23 INTERFACE "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY>:$<TARGET_OBJECTS:__cmake_cxx23>>")
    set_property(TARGET __cmake_cxx23 PROPERTY EXCLUDE_FROM_ALL 1)
    set_property(TARGET __cmake_cxx23 PROPERTY CXX_SCAN_FOR_MODULES 1)
    set_property(TARGET __cmake_cxx23 PROPERTY CXX_MODULE_STD 0)
    target_compile_features(__cmake_cxx23 PUBLIC cxx_std_23)
    target_compile_options(__cmake_cxx23 PRIVATE -Wno-reserved-module-identifier)
    target_include_directories(__cmake_cxx23 PRIVATE
      "${_emscripten_libcxx_path}"
      "/usr/local/Cellar/emscripten/4.0.23/libexec/cache/sysroot/include/c++/v1"
      "/usr/local/Cellar/emscripten/4.0.23/libexec/cache/sysroot/include")
    target_sources(__cmake_cxx23
      PUBLIC
      FILE_SET std TYPE CXX_MODULES
        BASE_DIRS "${_emscripten_libcxx_path}"
        FILES "${_emscripten_libcxx_path}/std.cppm" "${_emscripten_libcxx_path}/std.compat.cppm")
    add_library(__CMAKE::CXX23 ALIAS __cmake_cxx23)
    list(APPEND CMAKE_CXX_COMPILER_IMPORT_STD "23")
    message(STATUS "Manually created __CMAKE::CXX23 target for Emscripten")
  else()
    message(WARNING "Emscripten std.cppm not found at ${_emscripten_libcxx_path}")
  endif()
endif()

# Dependencies
find_package(lam_symbols REQUIRED)
find_package(lam_linearalgebra REQUIRED)

# Emscripten settings
if(EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX ".js")

  # WebGPU via emdawnwebgpu port (replaces old -sUSE_WEBGPU=1)
  add_compile_options(--use-port=emdawnwebgpu)
  add_link_options(
    --use-port=emdawnwebgpu
    -sASYNCIFY
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']
    -sALLOW_MEMORY_GROWTH=1
    --shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html
  )

  # Release build: optimize and strip debug info for smaller/harder to reverse-engineer output
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -flto -DNDEBUG)
    add_link_options(
      -O3
      -flto
      --closure 1
      -sSTRIP=1
    )
    message(STATUS "Release build: optimizations and stripping enabled")
  endif()
endif()

# VCpp sources (included directly for Emscripten build)
set(VCPP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

add_executable(vcpp_web_test)

target_sources(vcpp_web_test
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${VCPP_SRC_DIR}
    FILES
      ${VCPP_SRC_DIR}/vcpp.cppm
      ${VCPP_SRC_DIR}/vcpp-vec.cppm
      ${VCPP_SRC_DIR}/vcpp-color.cppm
      ${VCPP_SRC_DIR}/vcpp-props.cppm
      ${VCPP_SRC_DIR}/vcpp-traits.cppm
      ${VCPP_SRC_DIR}/vcpp-object-base.cppm
      ${VCPP_SRC_DIR}/vcpp-objects.cppm
      ${VCPP_SRC_DIR}/vcpp-scene.cppm
      ${VCPP_SRC_DIR}/vcpp-loop.cppm
      ${VCPP_SRC_DIR}/vcpp-input.cppm
      ${VCPP_SRC_DIR}/vcpp_wgpu_web.cppm
      web_test.cppm
  PRIVATE
    main.cpp
)

target_link_libraries(vcpp_web_test PRIVATE lam::symbols lam::linearalgebra)

# Copy HTML and LICENSE files to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index.html
               ${CMAKE_CURRENT_BINARY_DIR}/index.html COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE
               ${CMAKE_CURRENT_BINARY_DIR}/LICENSE COPYONLY)

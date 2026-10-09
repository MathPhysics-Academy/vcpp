cmake_minimum_required(VERSION 3.31.6 FATAL_ERROR)

# C++23 import-std UUID ladder + standard settings (must precede project())
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/LamCxxModuleStd.cmake)

project(vcpp_web_example LANGUAGES CXX)

# Emscripten needs manual __CMAKE::CXX23 target creation since CMake's
# detection doesn't find the std modules in Emscripten's toolchain
if(EMSCRIPTEN AND NOT TARGET "__CMAKE::CXX23")
  set(_emscripten_std_modules "${CMAKE_CURRENT_SOURCE_DIR}/../../lam/cmake/emscripten-std-modules")
  execute_process(COMMAND em-config CACHE OUTPUT_VARIABLE _emscripten_cache OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(_emscripten_libcxx_headers "${_emscripten_cache}/sysroot/include/c++/v1")
  set(_emscripten_sysroot "${_emscripten_cache}/sysroot/include")
  if(EXISTS "${_emscripten_std_modules}/std.cppm")
    add_library(__cmake_cxx23 STATIC)
    target_sources(__cmake_cxx23 INTERFACE "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY>:$<TARGET_OBJECTS:__cmake_cxx23>>")
    set_property(TARGET __cmake_cxx23 PROPERTY EXCLUDE_FROM_ALL 1)
    set_property(TARGET __cmake_cxx23 PROPERTY CXX_SCAN_FOR_MODULES 1)
    set_property(TARGET __cmake_cxx23 PROPERTY CXX_MODULE_STD 0)
    target_compile_features(__cmake_cxx23 PUBLIC cxx_std_23)
    target_compile_options(__cmake_cxx23 PRIVATE -Wno-reserved-module-identifier)
    target_include_directories(__cmake_cxx23 PRIVATE
      "${_emscripten_libcxx_headers}"
      "${_emscripten_sysroot}")
    target_sources(__cmake_cxx23
      PUBLIC
      FILE_SET std TYPE CXX_MODULES
        BASE_DIRS "${_emscripten_std_modules}"
        FILES "${_emscripten_std_modules}/std.cppm" "${_emscripten_std_modules}/std.compat.cppm")
    add_library(__CMAKE::CXX23 ALIAS __cmake_cxx23)
    list(APPEND CMAKE_CXX_COMPILER_IMPORT_STD "23")
    message(STATUS "Manually created __CMAKE::CXX23 target for Emscripten")
  else()
    message(WARNING "Emscripten std.cppm not found at ${_emscripten_std_modules}")
  endif()
endif()

# WebGPU port flag must reach every target, lam included: CMake 4.4 builds a separate std module
# per flag set, and a mismatch leaves two of them. emscan-deps (6.0.9) doesn't expand --use-port,
# so add its include explicitly.
if(EMSCRIPTEN)
  add_compile_options(--use-port=emdawnwebgpu)
  execute_process(COMMAND em-config CACHE OUTPUT_VARIABLE _em_cache OUTPUT_STRIP_TRAILING_WHITESPACE)
  include_directories(SYSTEM "${_em_cache}/ports/emdawnwebgpu/emdawnwebgpu_pkg/webgpu/include")
endif()

# Dependencies — prefer installed lam, fall back to building from submodule
find_package(lam_symbols QUIET)
find_package(lam_linearalgebra QUIET)
if(NOT lam_symbols_FOUND OR NOT lam_linearalgebra_FOUND)
  message(STATUS "lam not found installed; building from submodule")
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lam ${CMAKE_CURRENT_BINARY_DIR}/lam EXCLUDE_FROM_ALL)
endif()

# Emscripten settings
if(EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX ".js")

  # WebGPU via emdawnwebgpu port (replaces old -sUSE_WEBGPU=1); compile flag set above
  # Optional: a TrueType font for 3D text, embedded at /fonts/sans.ttf (GlowScript uses Roboto-Medium.ttf).
  # Fonts are third-party, so point this at a file outside the repo. Without it, 3D text uses simplified
  # built-in outlines.
  set(VCPP_FONT_SANS "" CACHE FILEPATH "TrueType font for 3D text")
  if(VCPP_FONT_SANS)
    if(NOT EXISTS "${VCPP_FONT_SANS}")
      message(FATAL_ERROR "VCPP_FONT_SANS: no file at ${VCPP_FONT_SANS}")
    endif()
    add_link_options("SHELL:--embed-file ${VCPP_FONT_SANS}@/fonts/sans.ttf")
  endif()

  # GlowScript's texture images (textures::wood and the rest) are third-party too: point this at a directory
  # holding them, such as GlowScript's glowscript_data. They are copied to textures/ beside the page, which
  # fetches them. Without it, those objects are drawn untextured.
  set(VCPP_TEXTURE_DIR "" CACHE PATH "Directory of GlowScript's texture images")
  if(VCPP_TEXTURE_DIR)
    file(GLOB _texture_images "${VCPP_TEXTURE_DIR}/*_texture.jpg")
    if(NOT _texture_images)
      message(FATAL_ERROR "VCPP_TEXTURE_DIR: no *_texture.jpg files in ${VCPP_TEXTURE_DIR}")
    endif()
    file(COPY ${_texture_images} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/textures)
  endif()

  add_link_options(
    --use-port=emdawnwebgpu
    -sASYNCIFY
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']
    -sALLOW_MEMORY_GROWTH=1
    --shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html
  )

  # EM_JS symbols from C++23 module interface units are marked live: 0 in the
  # ThinLTO summary, so wasm-ld's DCE strips them before binaryen can extract
  # JS wrappers. --undefined forces them into the LTO live-root set so they
  # survive; --export then ensures they appear in the WASM export table where
  # extract_metadata.py reads them as ExternType.GLOBAL entries.
  add_link_options(
    -Wl,--undefined=__em_js__js_create_graph
    -Wl,--undefined=__em_js__js_create_plot
    -Wl,--undefined=__em_js__js_add_point
    -Wl,--undefined=__em_js__js_add_points_batch
    -Wl,--undefined=__em_js__js_update_graph_bounds
    -Wl,--undefined=__em_js__js_redraw_graph
    -Wl,--undefined=__em_js__js_clear_plot
    -Wl,--undefined=__em_js__js_remove_graph
    -Wl,--undefined=__em_js__js_set_plot_visible
    -Wl,--undefined=__em_js__js_init_label_canvas
    -Wl,--undefined=__em_js__js_clear_labels
    -Wl,--undefined=__em_js__js_draw_label
    -Wl,--undefined=__em_js__js_load_texture
    -Wl,--undefined=__em_js__js_texture_width
    -Wl,--undefined=__em_js__js_texture_height
    -Wl,--undefined=__em_js__js_texture_take
    -Wl,--undefined=__em_js__js_set_scene_text
    -Wl,--export=__em_js__js_create_graph
    -Wl,--export=__em_js__js_create_plot
    -Wl,--export=__em_js__js_add_point
    -Wl,--export=__em_js__js_add_points_batch
    -Wl,--export=__em_js__js_update_graph_bounds
    -Wl,--export=__em_js__js_redraw_graph
    -Wl,--export=__em_js__js_clear_plot
    -Wl,--export=__em_js__js_remove_graph
    -Wl,--export=__em_js__js_set_plot_visible
    -Wl,--export=__em_js__js_init_label_canvas
    -Wl,--export=__em_js__js_clear_labels
    -Wl,--export=__em_js__js_draw_label
    -Wl,--export=__em_js__js_load_texture
    -Wl,--export=__em_js__js_texture_width
    -Wl,--export=__em_js__js_texture_height
    -Wl,--export=__em_js__js_texture_take
    -Wl,--export=__em_js__js_set_scene_text
  )

  # Release build: optimize and strip debug info for smaller/harder to reverse-engineer output
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Note: LTO (-flto) is intentionally omitted here. Clang's ThinLTO and Full
    # LTO both DCE the __em_js__* data globals from C++23 module interface units
    # (they appear as live:0 in the LTO summary despite being in @llvm.used).
    # Without LTO, wasm-ld's regular GC-based DCE correctly keeps these symbols
    # alive. Emscripten still runs binaryen wasm-opt at -O3 for output quality.
    add_compile_options(-O3 -DNDEBUG)
    add_link_options(
      -O3
      --closure 0
    )
    message(STATUS "Release build: optimizations and stripping enabled")
  endif()
endif()

# VCpp sources (included directly for Emscripten build)
set(VCPP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

set(VCPP_MODULE_FILES
  ${VCPP_SRC_DIR}/vcpp.cppm
  ${VCPP_SRC_DIR}/vcpp-vec.cppm
  ${VCPP_SRC_DIR}/vcpp-color.cppm
  ${VCPP_SRC_DIR}/vcpp-props.cppm
  ${VCPP_SRC_DIR}/vcpp-traits.cppm
  ${VCPP_SRC_DIR}/vcpp-render-types.cppm
  ${VCPP_SRC_DIR}/vcpp-render-resources.cppm
  ${VCPP_SRC_DIR}/vcpp-mesh.cppm
  ${VCPP_SRC_DIR}/vcpp-render-interface.cppm
  ${VCPP_SRC_DIR}/vcpp-object-base.cppm
  ${VCPP_SRC_DIR}/vcpp-objects.cppm
  ${VCPP_SRC_DIR}/vcpp-shapes.cppm
  ${VCPP_SRC_DIR}/vcpp-font.cppm
  ${VCPP_SRC_DIR}/vcpp-text-glyphs.cppm
  ${VCPP_SRC_DIR}/vcpp-label-bridge.cppm
  ${VCPP_SRC_DIR}/vcpp-events.cppm
  ${VCPP_SRC_DIR}/vcpp-scene.cppm
  ${VCPP_SRC_DIR}/vcpp-loop.cppm
  ${VCPP_SRC_DIR}/vcpp-coro.cppm
  ${VCPP_SRC_DIR}/vcpp-input.cppm
  ${VCPP_SRC_DIR}/vcpp-graph-base.cppm
  ${VCPP_SRC_DIR}/vcpp-graph-objects.cppm
  ${VCPP_SRC_DIR}/vcpp-graph-bridge.cppm
  ${VCPP_SRC_DIR}/vcpp_wgpu_web.cppm
)

add_executable(vcpp_web_test)

target_sources(vcpp_web_test
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${VCPP_SRC_DIR}
    FILES
      ${VCPP_MODULE_FILES}
      web_test.cppm
  PRIVATE
    main.cpp
)

target_link_libraries(vcpp_web_test PRIVATE lam::symbols lam::linearalgebra)

# Copy HTML, JS, and LICENSE files to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index.html
               ${CMAKE_CURRENT_BINARY_DIR}/index.html COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/graph_instance.js
               ${CMAKE_CURRENT_BINARY_DIR}/graph_instance.js COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/graph_manager.js
               ${CMAKE_CURRENT_BINARY_DIR}/graph_manager.js COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE
               ${CMAKE_CURRENT_BINARY_DIR}/LICENSE COPYONLY)

# Optional: build one VPython-style program, a .cpp defining `vcpp::task<void> vpython_program()`.
# It may live outside this repo. Output: <name>.html/.js/.wasm next to the demo.
set(VCPP_PROGRAM "" CACHE FILEPATH "Program to build alongside the demo")
if(VCPP_PROGRAM)
  get_filename_component(_prog ${VCPP_PROGRAM} NAME_WE)
  add_executable(${_prog})
  target_sources(${_prog}
    PUBLIC
      FILE_SET CXX_MODULES
      BASE_DIRS ${VCPP_SRC_DIR}
      FILES ${VCPP_MODULE_FILES}
    PRIVATE
      program_main.cpp
      ${VCPP_PROGRAM}
  )
  target_link_libraries(${_prog} PRIVATE lam::symbols lam::linearalgebra)
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/index.html _html)
  string(REPLACE "vcpp_web_test.js" "${_prog}.js" _html "${_html}")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${_prog}.html "${_html}")
endif()

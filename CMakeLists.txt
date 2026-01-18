
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
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(lam_vcpp VERSION 0.1.260118 LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Dependencies
find_package(lam_symbols REQUIRED)
find_package(lam_linearalgebra REQUIRED)

add_library(vcpp src/vcpp.cppm)
add_library(lam_vcpp::vcpp ALIAS vcpp)
target_sources(vcpp PUBLIC
  FILE_SET CXX_MODULES
  TYPE CXX_MODULES
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
    src/vcpp.cppm
    src/vcpp-vec.cppm
    src/vcpp-color.cppm
    src/vcpp-props.cppm
    src/vcpp-traits.cppm
    src/vcpp-object-base.cppm
    src/vcpp-objects.cppm
    src/vcpp-scene.cppm
    src/vcpp-loop.cppm
    src/vcpp-input.cppm
)
target_compile_features(vcpp PUBLIC cxx_std_23)
target_link_libraries(vcpp PUBLIC lam::symbols lam::linearalgebra)
set_target_properties(vcpp PROPERTIES OUTPUT_NAME lam_vcpp)

# CMake helpers needed for install
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Optional WebGPU rendering backend (separate module: vcpp.wgpu)
option(VCPP_BUILD_WGPU_BACKEND "Build WebGPU rendering backend" OFF)

if(VCPP_BUILD_WGPU_BACKEND)
  find_package(glfw3 REQUIRED)
  # wgpu-native doesn't have standard CMake config
  # Set WGPU_ROOT to point to wgpu-native installation

  add_library(vcpp_wgpu src/vcpp_wgpu.cppm)
  add_library(lam_vcpp::vcpp_wgpu ALIAS vcpp_wgpu)
  target_sources(vcpp_wgpu PUBLIC
    FILE_SET CXX_MODULES
    TYPE CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      src/vcpp_wgpu.cppm
  )
  target_compile_features(vcpp_wgpu PUBLIC cxx_std_23)
  target_link_libraries(vcpp_wgpu PUBLIC vcpp glfw)

  # Link wgpu-native
  if(DEFINED WGPU_ROOT)
    target_include_directories(vcpp_wgpu PUBLIC ${WGPU_ROOT}/include)
    target_link_directories(vcpp_wgpu PUBLIC ${WGPU_ROOT}/lib)
  endif()
  target_link_libraries(vcpp_wgpu PUBLIC wgpu_native)

  set_target_properties(vcpp_wgpu PROPERTIES OUTPUT_NAME lam_vcpp_wgpu)

  # macOS: add Objective-C++ helper for Metal surface
  if(APPLE)
    enable_language(OBJCXX)
    target_sources(vcpp_wgpu PRIVATE src/metal_surface.mm)
  endif()

  # macOS frameworks needed by wgpu
  if(APPLE)
    find_library(METAL_FRAMEWORK Metal)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
    find_library(APPKIT_FRAMEWORK AppKit)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(IOSURFACE_FRAMEWORK IOSurface)
    target_link_libraries(vcpp_wgpu PUBLIC
      ${METAL_FRAMEWORK}
      ${QUARTZCORE_FRAMEWORK}
      ${COREFOUNDATION_FRAMEWORK}
      ${COREGRAPHICS_FRAMEWORK}
      ${APPKIT_FRAMEWORK}
      ${IOKIT_FRAMEWORK}
      ${IOSURFACE_FRAMEWORK}
    )
  endif()

  # Install vcpp_wgpu
  install(TARGETS vcpp_wgpu
    EXPORT lam_vcpp_wgpuTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_LIBDIR}/c++/v1
  )

  install(EXPORT lam_vcpp_wgpuTargets
    FILE lam_vcpp_wgpuTargets.cmake
    NAMESPACE lam_vcpp_wgpu::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lam_vcpp_wgpu
  )

  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lam_vcpp_wgpuConfig.cmake" [[
include(CMakeFindDependencyMacro)
find_dependency(lam_vcpp)
find_dependency(glfw3)
include("${CMAKE_CURRENT_LIST_DIR}/lam_vcpp_wgpuTargets.cmake")
# Backward compatibility alias
if(TARGET lam_vcpp::vcpp_wgpu AND NOT TARGET lam::vcpp_wgpu)
  add_library(lam::vcpp_wgpu ALIAS lam_vcpp::vcpp_wgpu)
endif()
]])

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/lam_vcpp_wgpuConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/lam_vcpp_wgpuConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/lam_vcpp_wgpuConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lam_vcpp_wgpu
  )
endif()

# === INSTALLATION ===

install(TARGETS vcpp
  EXPORT lam_vcppTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_LIBDIR}/c++/v1
)

install(EXPORT lam_vcppTargets
  FILE lam_vcppTargets.cmake
  NAMESPACE lam_vcpp::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lam_vcpp
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lam_vcppConfig.cmake" [[
include(CMakeFindDependencyMacro)
find_dependency(lam_symbols)
find_dependency(lam_linearalgebra)
include("${CMAKE_CURRENT_LIST_DIR}/lam_vcppTargets.cmake")
# Backward compatibility alias
if(TARGET lam_vcpp::vcpp AND NOT TARGET lam::vcpp)
  add_library(lam::vcpp ALIAS lam_vcpp::vcpp)
endif()
]])

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/lam_vcppConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/lam_vcppConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/lam_vcppConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lam_vcpp
)

# Examples
option(VCPP_BUILD_EXAMPLES "Build examples" OFF)
if(VCPP_BUILD_EXAMPLES AND VCPP_BUILD_WGPU_BACKEND)
  add_subdirectory(examples)
endif()

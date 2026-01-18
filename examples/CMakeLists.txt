# Examples - built as subdirectory of vcpp

# Basic test executable
add_executable(basic_test)

target_sources(basic_test
  PUBLIC
    FILE_SET CXX_MODULES FILES
      basic_test.cppm
  PRIVATE
    main.cpp
)

target_link_libraries(basic_test
  PRIVATE
    vcpp
    vcpp_wgpu
)

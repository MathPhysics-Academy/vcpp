# VCpp Tests

add_executable(test_coro test_coro.cpp)
target_link_libraries(test_coro PRIVATE vcpp)
target_compile_features(test_coro PRIVATE cxx_std_23)

# Register with CTest
enable_testing()
add_test(NAME test_coro COMMAND test_coro)

add_executable(test_named_params test_named_params.cpp)
target_link_libraries(test_named_params PRIVATE vcpp)
target_compile_features(test_named_params PRIVATE cxx_std_23)
add_test(NAME test_named_params COMMAND test_named_params)

add_executable(test_trails test_trails.cpp)
target_link_libraries(test_trails PRIVATE vcpp)
target_compile_features(test_trails PRIVATE cxx_std_23)
add_test(NAME test_trails COMMAND test_trails)

add_executable(test_orientation test_orientation.cpp)
target_link_libraries(test_orientation PRIVATE vcpp)
target_compile_features(test_orientation PRIVATE cxx_std_23)
add_test(NAME test_orientation COMMAND test_orientation)

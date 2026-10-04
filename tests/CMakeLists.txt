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

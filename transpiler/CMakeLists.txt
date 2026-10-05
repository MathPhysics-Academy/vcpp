# vpy2cpp: VPython -> vcpp C++. Its only Python is vpy_ast.py, which parses with Python's own parser.
add_library(vpy2cpp_lib)
target_sources(vpy2cpp_lib
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      vpy2cpp.cppm
      json.cppm
      emit.cppm
)
target_compile_features(vpy2cpp_lib PUBLIC cxx_std_23)

add_executable(vpy2cpp main.cpp)
target_link_libraries(vpy2cpp PRIVATE vpy2cpp_lib)

# The translation of our own test program must match the reviewed expected output
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  enable_testing()
  add_test(NAME vpy2cpp_features
           COMMAND ${CMAKE_COMMAND} -DPYTHON=${Python3_EXECUTABLE} -DVPY_AST=${CMAKE_CURRENT_SOURCE_DIR}/vpy_ast.py
                   -DVPY2CPP=$<TARGET_FILE:vpy2cpp> -DPROGRAM=${CMAKE_CURRENT_SOURCE_DIR}/tests/features.py
                   -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/features.expected.cpp
                   -DWORK=${CMAKE_CURRENT_BINARY_DIR}/features -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_translation.cmake)
endif()

# Every named-parameter symbol in vcpp-props.cppm must be in the transpiler's reserved names, or a program
# variable of that name would clash with it in the generated C++.
# usage: cmake -DPROPS=<vcpp-props.cppm> -DEMIT=<emit.cppm> -P check_reserved.cmake
file(READ ${PROPS} props)
file(READ ${EMIT} emit)
string(REGEX MATCHALL "symbol<> [a-z_0-9]+\\{\\}" symbols "${props}")
set(missing "")
foreach(s ${symbols})
  string(REGEX REPLACE "symbol<> ([a-z_0-9]+)\\{\\}" "\\1" name "${s}")
  string(FIND "${emit}" "\"${name}\"" at)
  if(at EQUAL -1)
    list(APPEND missing ${name})
  endif()
endforeach()
if(missing)
  message(FATAL_ERROR "not in emit.cppm's reserved names: ${missing}")
endif()

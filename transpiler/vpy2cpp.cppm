/*
 *  vpy2cpp - Translates VPython programs into vcpp C++
 *
 *  Input is the JSON syntax tree from vpy_ast.py; output is a C++ file defining vpython_program(),
 *  which vcpp's run_program() runs (see VCPP_PROGRAM in examples/web).
 */

module;

import std;

export module vpy2cpp;

export import :json;
export import :emit;

export namespace vpy2cpp
{

// The C++ for a program, given vpy_ast.py's JSON for it; throws translate_error at the first statement
// vcpp can't express yet
inline std::string translate(std::string_view json_text)
{
  emitter e;
  return e.program(json_reader::parse(json_text));
}

} // namespace vpy2cpp

/*
 *  program_main.cpp - Entry point for a VPython-style program (see VCPP_PROGRAM)
 */

import vcpp;
import vcpp.wgpu.web;

vcpp::task<void> vpython_program(); // defined in the program's own file

int main() { return vcpp::wgpu::web::run_program(vpython_program); }

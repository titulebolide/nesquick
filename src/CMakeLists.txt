cmake_minimum_required (VERSION 3.5)
project (nesemu)
add_executable(nesemu utils.cpp lstdebugger.cpp ppu.cpp cpu.cpp cpumem.cpp main.cpp)

cmake_minimum_required(VERSION 3.18)
project(shogun_trap_bench C)
set(CMAKE_C_STANDARD 99)
# Point this at a unicorn built for the same ABI (see README).
set(UNICORN_ROOT "${CMAKE_SOURCE_DIR}/unicorn" CACHE PATH "unicorn install prefix")
add_executable(trapbench bench.c)
target_include_directories(trapbench PRIVATE ${UNICORN_ROOT}/include)
target_link_directories(trapbench PRIVATE ${UNICORN_ROOT}/lib)
target_link_libraries(trapbench unicorn)

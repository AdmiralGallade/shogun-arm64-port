cmake_minimum_required(VERSION 3.22)
project(shogun CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Headers and prebuilt libunicorn.so, both produced by the unicorn build.
set(UNICORN_INC "" CACHE PATH "unicorn include dir")
set(UNICORN_LIB "" CACHE FILEPATH "libunicorn.so for this ABI")

add_library(unicorn SHARED IMPORTED)
set_target_properties(unicorn PROPERTIES IMPORTED_LOCATION "${UNICORN_LIB}")

add_library(shogun SHARED
    entry.cpp
    shogun_runtime.cpp
    shims.cpp
    jni_bridge.cpp
    gl.cpp)

target_include_directories(shogun PRIVATE "${UNICORN_INC}")
target_compile_options(shogun PRIVATE -O2 -fno-exceptions -fno-rtti)
target_link_libraries(shogun unicorn GLESv1_CM EGL log android)

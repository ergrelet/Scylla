cmake_minimum_required(VERSION 3.15)

project(Scylla
    VERSION 0.9.8
    LANGUAGES C CXX
)

add_compile_definitions("UNICODE" "_UNICODE" "_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS")
# Static linkage with MSVC's runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory("Scylla")
add_subdirectory("thirdparty")
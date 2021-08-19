cmake_minimum_required(VERSION 3.15)

project(Scylla
    VERSION 0.9.8
    LANGUAGES C CXX
)

add_compile_definitions("UNICODE" "_UNICODE" "_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS")
# Static linkage with MSVC's runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("src")
add_subdirectory("thirdparty")
add_subdirectory("pyscylla")

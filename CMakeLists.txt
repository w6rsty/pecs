cmake_minimum_required(VERSION 3.20)

project(pecs LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(pecs INTERFACE)

target_include_directories(pecs INTERFACE include)

target_compile_features(pecs INTERFACE cxx_std_20)

add_subdirectory(sandbox)

if (APPLE) 
    set(CMAKE_CXX_COMPILER "clang++")
endif()

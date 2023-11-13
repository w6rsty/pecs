cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PECS_SRC src/entity/world.cpp)

add_library(pecs STATIC ${PECS_SRC})

target_include_directories(pecs PUBLIC include)

target_compile_features(pecs PRIVATE cxx_std_17)

add_subdirectory(test)

# set(CMAKE_CXX_COMPILER "clang++")

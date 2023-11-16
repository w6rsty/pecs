add_executable(pecs_sandbox main.cpp)

target_link_libraries(pecs_sandbox PRIVATE pecs)

target_compile_features(pecs_sandbox PRIVATE cxx_std_20)
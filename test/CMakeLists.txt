add_executable(pecs_test main.cpp)

target_link_libraries(pecs_test PRIVATE pecs)

target_compile_features(pecs_test PRIVATE cxx_std_20)
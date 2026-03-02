cmake_minimum_required(VERSION 3.16)
project(NaiveRefinedITMCD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(refined_itmcd
  src/refined_itmcd_algorithm.cpp
)
target_include_directories(refined_itmcd PUBLIC include)

add_executable(refined_itmcd_demo src/main.cpp)
target_link_libraries(refined_itmcd_demo PRIVATE refined_itmcd)

add_executable(test_refined_itmcd tests/test_refined_itmcd.cpp)
target_link_libraries(test_refined_itmcd PRIVATE refined_itmcd)

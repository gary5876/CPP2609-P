# Windows(Visual Studio), macOS, Linux 공용 빌드
cmake_minimum_required(VERSION 3.16)
project(HanSeek LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8 /W3)  # 소스의 한글 문자열을 UTF-8로 컴파일
else()
  add_compile_options(-Wall -Wextra)
endif()

add_library(miniz STATIC third_party/miniz/miniz.c)
target_include_directories(miniz PUBLIC third_party/miniz)

file(GLOB HANSEEK_SOURCES src/*.cpp)
list(REMOVE_ITEM HANSEEK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
add_library(hanseek_core STATIC ${HANSEEK_SOURCES})
target_include_directories(hanseek_core PUBLIC src)
target_link_libraries(hanseek_core PUBLIC miniz)

add_executable(hanseek src/main.cpp)
target_link_libraries(hanseek PRIVATE hanseek_core)

add_executable(hanseek_tests tests/test_main.cpp)
target_link_libraries(hanseek_tests PRIVATE hanseek_core)

enable_testing()
add_test(NAME unit COMMAND hanseek_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures)

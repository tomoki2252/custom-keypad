cmake_minimum_required(VERSION 3.20)
project(custom-keypad LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 高速化オプション（ある場合のみ）
find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER   ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic)
  # Debugビルド時のサニタイザ（必要に応じて有効化）
  # if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  #   add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  #   add_link_options(-fsanitize=address,undefined)
  # endif()
  # moldが入っていれば有効化
  find_program(MOLD_LINKER mold)
  if (MOLD_LINKER)
    add_link_options(-fuse-ld=mold)
  endif()
endif()

add_executable(custom-keypad src/main.cpp)

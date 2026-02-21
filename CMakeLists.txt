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
  find_program(MOLD_LINKER mold)
  if (MOLD_LINKER)
    add_link_options(-fuse-ld=mold)
  endif()
endif()

# Unicode (W-suffix API)
add_compile_definitions(UNICODE _UNICODE)

# UTF-8 source encoding
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

add_executable(custom-keypad WIN32
  src/main.cpp
  src/overlay.cpp
  src/hotkey.cpp
)

target_link_libraries(custom-keypad PRIVATE gdi32 user32)

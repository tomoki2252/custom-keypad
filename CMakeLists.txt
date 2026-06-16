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
  src/indicator.cpp
  src/switcher.cpp
  src/edge_flash.cpp
  src/actions.cpp
  src/config.cpp
  src/topmost.cpp
  src/mouse_hook.cpp
)

target_link_libraries(custom-keypad PRIVATE gdi32 user32)

# MinGWランタイムを静的リンクし、単体で起動できる exe にする
# （libstdc++-6.dll / libgcc_s_seh-1.dll への依存をなくす）
target_link_options(custom-keypad PRIVATE -static -static-libgcc -static-libstdc++)

# config.ini を実行ファイルと同じ場所へコピー（ビルド後）
add_custom_command(TARGET custom-keypad POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/config.ini
          $<TARGET_FILE_DIR:custom-keypad>/config.ini)

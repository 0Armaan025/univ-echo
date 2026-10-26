cmake_minimum_required(VERSION 3.20)

project(univ_echo
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SQLite3 REQUIRED IMPORTED_TARGET sqlite3)

add_executable(univecho
  src/main.cpp
  src/core/database.cpp
)

target_include_directories(univecho PRIVATE
  PkgConfig::SDL2
  PkgConfig::SQLite3
)


target_compile_options(univecho PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

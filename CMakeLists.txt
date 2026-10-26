cmake_minimum_required(VERSION 3.20)

project(univ_echo
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(NCURSESW REQUIRED REQUIRED IMPORTED_TARGET ncursesw)
find_package(SQLite3 REQUIRED)

add_executable(univecho
  src/main.cpp
  src/core/database.cpp
  src/core/command_history.cpp
)

target_include_directories(univecho PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(univecho PRIVATE

  PkgConfig::NCURSESW
  SQLite::SQLite3
)


target_compile_options(univecho PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

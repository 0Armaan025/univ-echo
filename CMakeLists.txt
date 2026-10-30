cmake_minimum_required(VERSION 3.20)

project(univ_echo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(univecho
    src/main.cpp
    src/core/database.cpp
    src/core/command_history.cpp
    src/core/semantic_search.cpp
    src/core/resource_manager.cpp
)

target_include_directories(univecho PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
  # PDCurses provides the curses API on Windows.
  find_path(PDCURSES_INCLUDE_DIR
        NAMES curses.h
        REQUIRED
    )

  find_library(PDCURSES_LIBRARY
        NAMES pdcurses
        REQUIRED
    )

  target_include_directories(univecho PRIVATE
        ${PDCURSES_INCLUDE_DIR}
    )

  target_link_libraries(univecho PRIVATE
        ${PDCURSES_LIBRARY}
    )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(NCURSESW REQUIRED IMPORTED_TARGET ncursesw)

  target_link_libraries(univecho PRIVATE
        PkgConfig::NCURSESW
    )
endif()

target_link_libraries(univecho PRIVATE
    SQLite3::SQLite3
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

if(MSVC)
  target_compile_options(univecho PRIVATE /W4 /permissive-)
else()
  target_compile_options(univecho PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

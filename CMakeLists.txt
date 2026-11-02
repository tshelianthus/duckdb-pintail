cmake_minimum_required(VERSION 3.8)

# Set extension name here
set(TARGET_NAME pintail)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GCC emits ODR-used static constexpr LogicalType members as STB_GNU_UNIQUE.
# aarch64 ld then rejects the clash with libduckdb_static. -fno-gnu-unique makes
# the extension copies weak so they resolve to Core's definition.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND UNIX AND NOT APPLE)
  add_compile_options(-fno-gnu-unique)
endif()

include_directories(src/include)

set(EXTENSION_SOURCES
    src/pintail_extension.cpp
    src/geohash/pintail_wkb.cpp)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")

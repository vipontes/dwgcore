cmake_minimum_required(VERSION 3.16)
project(dwgcore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# --- libdxfrw -----------------------------------------------------------
# Same reasoning as dwgviewer/CMakeLists.txt: don't add_subdirectory()
# libdxfrw's own CMakeLists.txt (it also builds the dwg2dxf CLI tool, a doc
# target, and a broken install/export step referencing a
# cmake/libdxfrwConfig.cmake.in that doesn't exist in this vendored copy).
# Build straight from its source manifest instead.
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/libdxfrw/libdxfrw_sources.cmake")

add_library(dxfrw STATIC ${LIBDXFRW_SOURCES} ${LIBDXFRW_PRIVATE_HEADERS} ${LIBDXFRW_PUBLIC_HEADERS})
target_compile_features(dxfrw PUBLIC cxx_std_17)
target_include_directories(dxfrw PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libdxfrw/src"
)

# --- Qt -------------------------------------------------------------------
# Prefer Qt6, fall back to Qt5 -- Qt6's official installer ships no 32-bit
# Windows kit, so a 32-bit build of this DLL (for a legacy 32-bit host
# process) requires Qt5, while a 64-bit host uses Qt6. Point
# CMAKE_PREFIX_PATH at the matching kit when configuring, e.g.:
#   64-bit: -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64
#   32-bit: -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019
find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
    set(QT_WIDGETS_TARGET Qt6::Widgets)
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)
    set(QT_WIDGETS_TARGET Qt5::Widgets)
endif()

# --- dwgcore --------------------------------------------------------------
add_library(dwgcore SHARED
    src/dwg_document.cpp
    src/dwg_document.h
    src/viewer_widget.cpp
    src/viewer_widget.h
    src/qt_engine.cpp
    src/qt_engine.h
    src/dwg_view_session.cpp
    src/dwg_view_session.h
    src/dwgcore_api.cpp
    src/dwgcore_api.h
)
target_include_directories(dwgcore PRIVATE src)
target_compile_definitions(dwgcore PRIVATE DWGCORE_EXPORTS)
target_link_libraries(dwgcore PRIVATE ${QT_WIDGETS_TARGET} dxfrw)

cmake_minimum_required(VERSION 3.22)

option(QUEST "Build for quest" ON)

# Globals
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_compile_options(-frtti -fexceptions -fvisibility=hidden -fPIE -fPIC)

# Include. Include order matters!
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/utils.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/qpm.cmake)

if(${CMAKE_BUILD_TYPE} STREQUAL "RELEASE" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo" OR ${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
        # Better optimizations
        add_compile_options(-O3)

        # LTO
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        add_compile_options(-flto)
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "DEBUG" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
        add_compile_options(-g)
endif()

# Targets
if(QUEST)
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/targets/quest.cmake)
endif()

# Post build
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/strip.cmake)

# stop symbols leaking
# TODO: Fix
# add_link_options(-Wl, --exclude-libs, ALL)
project( bstheater
        VERSION ${PACKAGE_VERSION})
enable_language(ASM)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/java/bstheater-download.dex" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/java/bstheater-desugar.dex")
    message(FATAL_ERROR "Build the Android downloader first: pwsh scripts/build-java.ps1")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/download_dex.S.in ${CMAKE_CURRENT_BINARY_DIR}/download_dex.S @ONLY)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/download_dex.S PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/java/bstheater-download.dex;${CMAKE_CURRENT_SOURCE_DIR}/java/bstheater-desugar.dex")

add_compile_definitions(MOD_ID="${CMAKE_PROJECT_NAME}")
add_compile_definitions(VERSION="${CMAKE_PROJECT_VERSION}")

# Set COMPILE_ID for qpm purposes
set(COMPILE_ID ${CMAKE_PROJECT_NAME})

# recursively get all src files
RECURSE_FILES(cpp_file_list ${SOURCE_DIR}/*.cpp)
RECURSE_FILES(c_file_list ${SOURCE_DIR}/*.c)
list(REMOVE_ITEM cpp_file_list ${SOURCE_DIR}/quickjs_solver.cpp)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ejs.S.in ${CMAKE_CURRENT_BINARY_DIR}/ejs.S @ONLY)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/ejs.S PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/ejs/yt.solver.lib.js;${CMAKE_CURRENT_SOURCE_DIR}/resources/ejs/yt.solver.core.js")
set(QUICKJS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/quickjs)
add_library(bstheaterjs SHARED src/quickjs_solver.cpp ${CMAKE_CURRENT_BINARY_DIR}/ejs.S
    ${QUICKJS_DIR}/quickjs.c ${QUICKJS_DIR}/cutils.c ${QUICKJS_DIR}/libregexp.c
    ${QUICKJS_DIR}/libunicode.c ${QUICKJS_DIR}/dtoa.c)
target_include_directories(bstheaterjs PRIVATE ${QUICKJS_DIR})
target_compile_definitions(bstheaterjs PRIVATE CONFIG_VERSION="2026-06-04" _GNU_SOURCE)
target_link_libraries(bstheaterjs PRIVATE m)
target_link_options(bstheaterjs PRIVATE -Wl,--strip-all)

add_library(
        ${CMAKE_PROJECT_NAME}
        SHARED
        ${cpp_file_list}
        ${c_file_list}
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/download_dex.S)

# for inline hook
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE -llog)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE bstheaterjs)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${INCLUDE_DIR})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${SHARED_DIR})

# beatsaber hook inline hook

RECURSE_FILES(src_inline_hook_beatsaber_hook_local_extra_c ${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.c)
RECURSE_FILES(src_inline_hook_beatsaber_hook_local_extra_cpp ${EXTERN_DIR}/includes/beatsaber-hook/shared/inline-hook/*.cpp)

target_sources(${COMPILE_ID} PRIVATE ${src_inline_hook_beatsaber_hook_local_extra_c})
target_sources(${COMPILE_ID} PRIVATE ${src_inline_hook_beatsaber_hook_local_extra_cpp})

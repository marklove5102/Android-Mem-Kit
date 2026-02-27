cmake_minimum_required(VERSION 3.14)
project(memkit C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================================
# Android NDK Setup
# ============================================================================

if(DEFINED ENV{ANDROID_NDK_HOME})
    set(ANDROID_NDK $ENV{ANDROID_NDK_HOME})
elseif(DEFINED ENV{NDK_HOME})
    set(ANDROID_NDK $ENV{NDK_HOME})
endif()

if(NOT ANDROID_NDK)
    message(FATAL_ERROR "ANDROID_NDK_HOME or NDK_HOME environment variable must be set")
endif()

message(STATUS "Android NDK: ${ANDROID_NDK}")

# ============================================================================
# Compiler Flags
# ============================================================================

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fPIC")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -llog")

# ============================================================================
# Option: Use local dependencies or fetch from git
# ============================================================================

option(USE_LOCAL_DEPS "Use local dependencies from deps/ folder" ON)

# ============================================================================
# Include Directories
# ============================================================================

include_directories(${CMAKE_SOURCE_DIR}/include)

# ============================================================================
# Dependencies
# ============================================================================

if(USE_LOCAL_DEPS)
    # Use local dependencies (faster, offline builds)

    # XDL library
    set(XDL_DIR ${CMAKE_SOURCE_DIR}/deps/xdl)
    if(EXISTS ${XDL_DIR})
        include_directories(${XDL_DIR}/include)
        file(GLOB XDL_SOURCES ${XDL_DIR}/src/*.c)
        add_library(xdl STATIC ${XDL_SOURCES})
        target_compile_definitions(xdl PRIVATE __ANDROID__API__=21)
        message(STATUS "Using local XDL: ${XDL_DIR}")
    else()
        message(WARNING "Local XDL not found. Run ./setup.sh or set USE_LOCAL_DEPS=OFF")
    endif()

    # ShadowHook library
    set(SHADOWHOOK_DIR ${CMAKE_SOURCE_DIR}/deps/shadowhook)
    if(EXISTS ${SHADOWHOOK_DIR})
        include_directories(${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/include)
        file(GLOB SHADOWHOOK_SOURCES
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/*.c
            ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/*.cc
        )
        add_library(shadowhook STATIC ${SHADOWHOOK_SOURCES})
        target_compile_definitions(shadowhook PRIVATE __ANDROID__API__=21)
        message(STATUS "Using local ShadowHook: ${SHADOWHOOK_DIR}")
    else()
        message(WARNING "Local ShadowHook not found. Run ./setup.sh or set USE_LOCAL_DEPS=OFF")
    endif()

else()
    # Fetch from git (requires internet, slower first build)

    include(FetchContent)

    # XDL
    FetchContent_Declare(
        xdl
        GIT_REPOSITORY https://github.com/hexhacking/xdl.git
        GIT_TAG        v1.2.1
    )

    # ShadowHook
    FetchContent_Declare(
        shadowhook
        GIT_REPOSITORY https://github.com/bytedance/android-inline-hook.git
        GIT_TAG        v1.3.0
    )

    message(STATUS "Fetching dependencies from git...")
    FetchContent_MakeAvailable(xdl shadowhook)
endif()

# ============================================================================
# Main memkit Library
# ============================================================================

add_library(memkit SHARED
    src/memory.c
    src/hooking.c
    src/il2cpp.c
)

target_include_directories(memkit PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${XDL_DIR}/include
    ${SHADOWHOOK_DIR}/shadowhook/src/main/cpp/include
)

target_link_libraries(memkit
    xdl
    shadowhook
    log
    android
)

set_target_properties(memkit PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ============================================================================
# Example (Optional)
# ============================================================================

option(BUILD_EXAMPLES "Build example projects" OFF)

if(BUILD_EXAMPLES)
    add_library(memkit-example SHARED
        examples/main.c
    )
    target_link_libraries(memkit-example memkit)
    set_target_properties(memkit-example PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/example"
    )
    message(STATUS "Building examples")
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS memkit
    LIBRARY DESTINATION lib
)

install(FILES include/memkit.h
    DESTINATION include
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "=== Android-Mem-Kit Configuration ===")
message(STATUS "  Android NDK: ${ANDROID_NDK}")
message(STATUS "  Local deps:  ${USE_LOCAL_DEPS}")
message(STATUS "  Examples:    ${BUILD_EXAMPLES}")
message(STATUS "=======================================")
message(STATUS "")

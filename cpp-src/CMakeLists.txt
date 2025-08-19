cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -ffast-math")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
project(rtp_player VERSION 1.0.0)

# Compiler settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include header directories
include_directories(include)

# Find necessary libraries
find_package(PkgConfig REQUIRED)

# Find libdrm
pkg_check_modules(DRM REQUIRED libdrm)

# Add uvgRTP as an external dependency
include(FetchContent)
FetchContent_Declare(
    uvgRTP
    GIT_REPOSITORY https://github.com/ultravideo/uvgRTP.git
    GIT_TAG master
)

# Disable crypto for uvgRTP for maximum performance
set(UVGRTP_DISABLE_CRYPTO ON CACHE BOOL "Disable crypto for uvgRTP")
set(UVGRTP_DISABLE_TESTS ON CACHE BOOL "Do not build unit tests")
set(UVGRTP_DISABLE_EXAMPLES ON CACHE BOOL "Do not build examples")
set(UVGRTP_DISABLE_INSTALL ON CACHE BOOL "Do not install anything from uvgRTP")


FetchContent_MakeAvailable(uvgRTP)


# --- Component Library ---
add_library(rtp_components STATIC
    src/lib/v4l2_decoder.cpp
    src/lib/v4l2_device.cpp
    src/lib/dma_buffers_manager.cpp
    src/lib/dmabuf_allocator.cpp
    src/lib/drm_dmabuf_display.cpp
    src/lib/uvgrtp_receiver.cpp
    src/lib/frame_processor.cpp
    src/lib/streaming_manager.cpp
)

target_include_directories(rtp_components PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --- uvgRTP ---
include(FetchContent)

# Link rtp_components with uvgrtp to make headers available
target_link_libraries(rtp_components PRIVATE uvgrtp)

# Add compile flags and include paths for rtp_components
target_compile_options(rtp_components PRIVATE ${DRM_CFLAGS_OTHER})
target_include_directories(rtp_components PRIVATE ${DRM_INCLUDE_DIRS})

# Define application source files
set(RTP_PLAYER_APP_SOURCES
    src/app/rtp_player.cpp
)

# Create the main RTP Player executable
add_executable(rtp_player ${RTP_PLAYER_APP_SOURCES})

# Link libraries
target_link_libraries(rtp_player 
    rtp_components
    ${DRM_LIBRARIES}
    uvgrtp
    pthread
)

# Add compile flags
target_compile_options(rtp_player PRIVATE ${DRM_CFLAGS_OTHER})
target_include_directories(rtp_player PRIVATE ${DRM_INCLUDE_DIRS})

# Installation
install(TARGETS rtp_player DESTINATION bin)

# Build information
message(STATUS "=== RTP Player Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "libdrm found: ${DRM_FOUND}")
message(STATUS "uvgRTP library: ENABLED")
message(STATUS "=================================")
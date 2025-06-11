# SPDX-FileCopyrightText: Copyright (c) 2024-2025 Infineon Technologies AG
# SPDX-License-Identifier: MIT

# ##############################################################################
# Project configuration and settings
# ##############################################################################
cmake_minimum_required(VERSION 3.14)


# Disable in-source builds
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Main project configuration
project(
  nbt-rpi
  VERSION 1.0.0
  DESCRIPTION "OPTIGA Authenticate NBT project for Raspberry Pi"
  LANGUAGES C)

# Compiler settings
set(CMAKE_C_STANDARD 99)

# Disable building documentation
set(BUILD_DOCUMENTATION OFF)

# TODO: Uncomment later
# # Downlload all the submodules in the project
# find_package(Git QUIET)
# if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
#     # Update submodules as needed
#     option(GIT_SUBMODULE "Check submodules during build" ON)
#     if(GIT_SUBMODULE)
#         message(STATUS "Submodule update")
#         execute_process(
#             COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
#             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#             RESULT_VARIABLE GIT_SUBMOD_RESULT
#         )
#         if(NOT GIT_SUBMOD_RESULT EQUAL "0")
#             message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
#         endif()
#     endif()
# endif()

# # Check if the submodules are downloaded
# if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/hsw-nbt/CMakeLists.txt")
#     message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
# endif()

# Set the source files
set(SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/sources/utilities/nbt-utilities.c"
)

set(HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/sources/utilities/nbt-utilities.h"
)


# Add optiga-nbt-lib-c folder as subdirectory (in external folder)
add_subdirectory(external/hsw-nbt)

# Add RPI port directory as subdirectory
add_subdirectory(extra/optiga-nbt-rpi-port)

# Add executable (main compiled from main.c)
add_executable(nbt-rpi source/main.c)
target_sources(nbt-rpi PRIVATE source/utilities/nbt-utilities.c)

target_link_libraries(nbt-rpi Infineon::hsw-apdu-nbt Infineon::hsw-ndef Infineon::hsw-protocol Infineon::hsw-t1prime Infineon::hsw-utils Infineon::optiga-nbt-rpi-port pthread)

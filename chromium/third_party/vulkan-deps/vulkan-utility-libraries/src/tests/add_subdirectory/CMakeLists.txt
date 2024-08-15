# Copyright 2023 The Khronos Group Inc.
# Copyright 2023 Valve Corporation
# Copyright 2023 LunarG, Inc.
#
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.17)

project(TEST_ADD_SUBDIRECTORY LANGUAGES C)

if (NOT DEFINED VULKAN_HEADER_SOURCE_DIR)
    message(FATAL_ERROR "VULKAN_HEADER_SOURCE_DIR not defined!")
endif()

add_subdirectory(${VULKAN_HEADER_SOURCE_DIR} ${PROJECT_BINARY_DIR}/headers)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../ ${PROJECT_BINARY_DIR}/vul)

get_filename_component(VUL_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../../include/" ABSOLUTE)

# Get all the header files we SHIP
file(GLOB_RECURSE public_headers
    CONFIGURE_DEPENDS
    "${VUL_INCLUDE_DIR}/*.h" "${VUL_INCLUDE_DIR}/*.hpp"
)

# Ensure each file has the same `vk_` prefix naming convention.
foreach(header IN LISTS public_headers)
    get_filename_component(header ${header} "NAME")
    message(DEBUG "Checking ${header}")
    if (${header} MATCHES "^vk_")
        message(DEBUG "Correct naming convention!")
    else()
        # EX: "Invalid file name! vk_dispatch_table.h"
        message(FATAL_ERROR "Invalid file name! ${header}")
    endif()
endforeach()

# Ensure gn stub file contains all public headers.
# Otherwise we can break the GN build by accident.
get_filename_component(gn_stub_file "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/gn/stub.cpp" ABSOLUTE)
if (NOT EXISTS ${gn_stub_file})
    message(FATAL_ERROR "Couldn't find gn stub file!")
endif()

file(READ ${gn_stub_file} gn_stub_contents)

foreach(header IN LISTS public_headers)
    get_filename_component(header ${header} "NAME")
    if (gn_stub_contents MATCHES "${header}>")
        message(DEBUG "Stub contains ${header}")
    else()
        message(FATAL_ERROR "Update ${gn_stub_file} with ${header}!")
    endif()
endforeach()

# The intent is ensuring we don't accidentally change the names of these
# targets. Since it would break downstream users.
if (NOT TARGET Vulkan::LayerSettings)
    message(FATAL_ERROR "Vulkan::LayerSettings target not defined!")
endif()
if (NOT TARGET Vulkan::UtilityHeaders)
    message(FATAL_ERROR "Vulkan::UtilityHeaders target not defined!")
endif()

add_library(add_subdirectory_example STATIC)

# Test c99 support
target_compile_features(add_subdirectory_example PRIVATE c_std_99)

target_sources(add_subdirectory_example PRIVATE
    vk_dispatch_table.c
    vk_enum_string_helper.c
    vk_layer_settings.c
    vk_format_utils.c
    vk_format_utils_2.c # Need two translation units to test if header file behaves correctly.
)

target_link_libraries(add_subdirectory_example PRIVATE
    Vulkan::LayerSettings
    Vulkan::UtilityHeaders
)

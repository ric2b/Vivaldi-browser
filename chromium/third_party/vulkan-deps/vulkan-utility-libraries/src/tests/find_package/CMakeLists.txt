# Copyright 2023 The Khronos Group Inc.
# Copyright 2023 Valve Corporation
# Copyright 2023 LunarG, Inc.
#
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.17)

project(TEST_FIND_PACKAGE LANGUAGES C)

add_library(find_package_example STATIC)

# Test c99 support as well as find_package support
target_compile_features(find_package_example PRIVATE c_std_99)

target_sources(find_package_example PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../add_subdirectory/vk_layer_settings.c
    ${CMAKE_CURRENT_LIST_DIR}/../add_subdirectory/vk_enum_string_helper.c
    ${CMAKE_CURRENT_LIST_DIR}/../add_subdirectory/vk_dispatch_table.c
    ${CMAKE_CURRENT_LIST_DIR}/../add_subdirectory/vk_format_utils.c
)

# NOTE: Because VulkanHeaders is a PUBLIC dependency it needs to be found prior to VulkanUtilityLibraries
find_package(VulkanHeaders REQUIRED CONFIG)

find_package(VulkanUtilityLibraries REQUIRED CONFIG)

if (NOT TARGET Vulkan::LayerSettings)
    message(FATAL_ERROR "Vulkan::LayerSettings target not defined!")
endif()

if (NOT TARGET Vulkan::UtilityHeaders)
    message(FATAL_ERROR "Vulkan::UtilityHeaders target not defined!")
endif()

target_link_libraries(find_package_example PRIVATE
    Vulkan::LayerSettings
    Vulkan::UtilityHeaders
)

# NOTE: Because Vulkan::Headers header files are exposed in the public facing interface
# we must expose this library to users.
get_target_property(property Vulkan::LayerSettings INTERFACE_LINK_LIBRARIES)
if (NOT property MATCHES "Vulkan::Headers")
    message(FATAL_ERROR "Vulkan::Headers not linked properly!")
endif()
get_target_property(property Vulkan::UtilityHeaders INTERFACE_LINK_LIBRARIES)
if (NOT property MATCHES "Vulkan::Headers")
    message(FATAL_ERROR "Vulkan::Headers not linked properly!")
endif()

# Prevent regression of https://github.com/KhronosGroup/Vulkan-Utility-Libraries/issues/103
get_target_property(property Vulkan::LayerSettings INTERFACE_COMPILE_OPTIONS)
if (NOT property STREQUAL "property-NOTFOUND")
    message(FATAL_ERROR "Vulkan::LayerSettings shouldn't export compile options! ${property}")
endif()
get_target_property(property Vulkan::UtilityHeaders INTERFACE_COMPILE_OPTIONS)
if (NOT property STREQUAL "property-NOTFOUND")
    message(FATAL_ERROR "Vulkan::UtilityHeaders shouldn't export compile options! ${property}")
endif()

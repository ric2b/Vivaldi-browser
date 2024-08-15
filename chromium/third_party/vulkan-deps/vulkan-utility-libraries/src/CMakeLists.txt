# Copyright 2023 The Khronos Group Inc.
# Copyright 2023 Valve Corporation
# Copyright 2023 LunarG, Inc.
#
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.17.2)

project(VUL LANGUAGES CXX)

string(COMPARE EQUAL ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR} VUL_IS_TOP_LEVEL) # Remove when min is 3.21

set_property(GLOBAL PROPERTY USE_FOLDERS ON) # Remove when min is 3.26, see CMP0143

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN "YES")

# This variable enables downstream users to customize the target API
# variant (e.g. Vulkan SC)
set(API_TYPE "vulkan")

add_subdirectory(scripts)

find_package(VulkanHeaders CONFIG QUIET)

add_subdirectory(src)
add_subdirectory(include)

if (VUL_IS_TOP_LEVEL)
    option(BUILD_TESTS "Build tests")
    if (BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()

    include(GNUInstallDirs)

    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/${API_TYPE}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/vulkan"
        PATTERN ".clang-format" EXCLUDE
    )

    set_target_properties(VulkanLayerSettings PROPERTIES EXPORT_NAME "LayerSettings")
    set_target_properties(VulkanUtilityHeaders PROPERTIES EXPORT_NAME "UtilityHeaders")
    set_target_properties(VulkanCompilerConfiguration PROPERTIES EXPORT_NAME "CompilerConfiguration")
    install(
        TARGETS VulkanLayerSettings VulkanUtilityHeaders VulkanCompilerConfiguration
        EXPORT VulkanUtilityLibrariesConfig
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(
        EXPORT VulkanUtilityLibrariesConfig
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VulkanUtilityLibraries
        NAMESPACE Vulkan::
    )
endif()

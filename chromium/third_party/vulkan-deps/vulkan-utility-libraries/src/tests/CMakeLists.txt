# Copyright 2023 The Khronos Group Inc.
# Copyright 2023 Valve Corporation
# Copyright 2023 LunarG, Inc.
#
# SPDX-License-Identifier: Apache-2.0

find_package(GTest REQUIRED CONFIG)
find_package(magic_enum REQUIRED CONFIG)

include(GoogleTest)

add_executable(vul_tests)

target_include_directories(vul_tests PRIVATE
    ${VUL_SOURCE_DIR}/src/layer
)

target_sources(vul_tests PRIVATE
    struct_helper.cpp
    test_formats.cpp
    test_interface.cpp
    test_setting_api.cpp
    test_setting_cast.cpp
    test_setting_cpp.cpp
    test_setting_env.cpp
    test_setting_file.cpp
    test_setting_util.cpp
    vk_enum_string_helper.cpp
)

target_link_libraries(vul_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    magic_enum::magic_enum
    Vulkan::UtilityHeaders
    Vulkan::LayerSettings
    Vulkan::CompilerConfiguration
)

if (CMAKE_CROSSCOMPILING)
    return()
endif()

gtest_discover_tests(vul_tests DISCOVERY_TIMEOUT 100)

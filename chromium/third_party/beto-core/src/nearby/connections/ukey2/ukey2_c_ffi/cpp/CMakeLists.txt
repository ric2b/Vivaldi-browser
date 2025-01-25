# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.14)

include_directories(
    ${CMAKE_SOURCE_DIR}/ukey2_c_ffi/cpp/)
include(ExternalProject)

set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/target/tmp)
ExternalProject_Add(
    ukey2_c_ffi
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build COMMAND cargo build --release --lib
    BINARY_DIR "${NEARBY_ROOT}/connections/ukey2/ukey2_c_ffi"
    INSTALL_COMMAND "")

add_executable(ukey2_ffi_test
  ukey2_test.cc
  ukey2_glue.cc
  ukey2_ffi.h
  ukey2_bindings.h)

set(ukey2_c_ffi_FILENAME "${CMAKE_SHARED_LIBRARY_PREFIX}ukey2_c_ffi${CMAKE_SHARED_LIBRARY_SUFFIX}")

if(UNIX)
    target_link_libraries(
        ukey2_ffi_test
        "${NEARBY_ROOT}/target/release/${ukey2_c_ffi_FILENAME}"
        GTest::gtest_main
        dl pthread
    )
elseif(MSVC)
    # MSVC requires linking to a static lib, which rust kindly generates
    target_link_libraries(
        ukey2_ffi_test
        "${NEARBY_ROOT}/target/release/${ukey2_c_ffi_FILENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}"
        GTest::gtest_main
    )
    # MSVC requires that the dynamic lib be visible to the binary, and copying to the binary dir is an easy way to do that
    get_target_property(ukey2_ffi_test_BINARY_DIR ukey2_ffi_test BINARY_DIR)
    add_custom_command(
        TARGET ukey2_ffi_test POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy "${NEARBY_ROOT}/target/release/${ukey2_c_ffi_FILENAME}" "${ukey2_ffi_test_BINARY_DIR}/."
    )
endif()

gtest_discover_tests(ukey2_ffi_test)

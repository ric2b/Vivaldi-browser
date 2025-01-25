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

project(Nearby)

set(CMAKE_CXX_STANDARD 20)

set(BETO_CORE_ROOT ${CMAKE_SOURCE_DIR}/..)
set(NEARBY_ROOT ${CMAKE_SOURCE_DIR})
set(THIRD_PARTY_DIR ${BETO_CORE_ROOT}/third_party)

set(CMAKE_C_FLAGS_DEBUG "-DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG")
if (UNIX)
    set(CMAKE_C_FLAGS_DEBUG "-g ${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_DEBUG "-g ${CMAKE_C_FLAGS_DEBUG}")
endif ()

if (MSVC)
    add_compile_options(-W4 -O1 -MD)
endif ()

if (ENABLE_TESTS)
    message(STATUS "Enabling workspace wide tests")

    # We want to suppress warnings coming from external libraries as they just
    # pollute the build output
    add_compile_options(-w)

    # Setup GoogleTest
    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    include(GoogleTest)

    # Include google benchmark
    add_subdirectory(${THIRD_PARTY_DIR}/benchmark ${THIRD_PARTY_DIR}/benchmark/build)

    # Setup jsoncpp
    set(JSONCPP_DIR ${THIRD_PARTY_DIR}/jsoncpp)
    include_directories(${JSONCPP_DIR})
    add_library(
            jsoncpp
            ${JSONCPP_DIR}/jsoncpp.cpp
    )
endif ()

set(ABSL_PROPAGATE_CXX_STD ON)
if (ENABLE_FUZZ)
    if (NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        MESSAGE(FATAL_ERROR "fuzzing is only enabed when building with Clang, please set CC and CXX to use clang instead of ${CMAKE_CXX_COMPILER_ID}")
    endif ()

    add_subdirectory(${THIRD_PARTY_DIR}/boringssl boringssl-build)
    add_subdirectory(${THIRD_PARTY_DIR}/fuzztest fuzztest-build)
    enable_testing()
    include(GoogleTest)
else ()
    # fuzztest handles pulling in absl, so we if we are not building fuzzers we need
    # to include it ourselves via third_party.
    add_subdirectory(${THIRD_PARTY_DIR}/abseil-cpp ${THIRD_PARTY_DIR}/abseil-cpp/build)
endif ()

if (UNIX)
    set_directory_properties(
            PROPERTIES
            COMPILE_OPTIONS
            -Werror
            -Wall
            -Wextra
            -Wimplicit-fallthrough
            -Wextra-semi
            -Wshadow
            -Wsign-compare
    )
endif ()

# rust std lib requires linking against these
if (UNIX)
    link_libraries(
            dl pthread -fsanitize=address
    )
elseif (MSVC)
    link_libraries(
            bcrypt ntdll userenv ws2_32
    )
endif ()

add_subdirectory(presence/np_cpp_ffi)
add_subdirectory(presence/ldt_np_adv_ffi/c)
add_subdirectory(connections/ukey2/ukey2_c_ffi/cpp/)

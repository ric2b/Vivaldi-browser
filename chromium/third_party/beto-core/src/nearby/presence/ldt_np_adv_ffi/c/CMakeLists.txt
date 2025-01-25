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

# Use ExternalProject to ensure that the rust artifacts are built
include(ExternalProject)
set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/target/tmp)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_ARTIFACT_DIR debug)
    set(CARGO_PROFILE dev)
else ()
    set(CARGO_ARTIFACT_DIR release)
    set(CARGO_PROFILE release)
endif ()

ExternalProject_Add(
        ldt_np_adv_ffi
        DOWNLOAD_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND cargo build -p ldt_np_adv_ffi --no-default-features --features std --profile=${CARGO_PROFILE}
        BUILD_BYPRODUCTS ${NEARBY_ROOT}/target/${CARGO_ARTIFACT_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}ldt_np_adv_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}
        BINARY_DIR "${NEARBY_ROOT}/target/"
        INSTALL_COMMAND "")

include_directories(${NEARBY_ROOT}/presence/ldt_np_adv_ffi/c/include/)

add_subdirectory(sample)

if (ENABLE_TESTS)
    message(STATUS "Enabling ldt tests")
    add_subdirectory(tests)
    add_subdirectory(benchmarks)
endif ()

# Fuzzers won't build on windows so we need a separate flag to pull them in
if (ENABLE_FUZZ)
    message(STATUS "Enabling ldt fuzzers")
    add_subdirectory(fuzz)
endif ()

/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#include "avif/avif.h"
#include "avif/libavif_compat.h"
#include "gtest/gtest.h"

using namespace crabbyavif;

// Used instead of CHECK if needing to return a specific error on failure,
// instead of AVIF_FALSE
#define AVIF_CHECKERR(A, ERR) \
  do {                        \
    if (!(A)) {               \
      return ERR;             \
    }                         \
  } while (0)

// Forward any error to the caller now or continue execution.
#define AVIF_CHECKRES(A)              \
  do {                                \
    const avifResult result__ = (A);  \
    if (result__ != AVIF_RESULT_OK) { \
      return result__;                \
    }                                 \
  } while (0)

namespace avif {

// Struct to call the destroy functions in a unique_ptr.
struct UniquePtrDeleter {
  void operator()(avifDecoder* decoder) const { avifDecoderDestroy(decoder); }
  void operator()(avifImage * image) const { avifImageDestroy(image); }
};

// Use these unique_ptr to ensure the structs are automatically destroyed.
using DecoderPtr = std::unique_ptr<avifDecoder, UniquePtrDeleter>;
using ImagePtr = std::unique_ptr<avifImage, UniquePtrDeleter>;

}  // namespace avif

namespace testutil {

bool Av1DecoderAvailable() { return true; }

std::vector<uint8_t> read_file(const char* file_name) {
  std::ifstream file(file_name, std::ios::binary);
  EXPECT_TRUE(file.is_open());
  // Get file size.
  file.seekg(0, std::ios::end);
  auto size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char*>(data.data()), size);
  file.close();
  return data;
}

}  // namespace testutil

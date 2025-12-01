/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/base/compression.h"

#include <cstddef>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/io/gzip_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace fcp {
using ::google::protobuf::io::ArrayInputStream;
using ::google::protobuf::io::GzipInputStream;
using ::google::protobuf::io::GzipOutputStream;
using ::google::protobuf::io::StringOutputStream;

absl::StatusOr<std::string> CompressWithGzip(
    absl::string_view uncompressed_data) {
  int starting_pos = 0;
  size_t str_size = uncompressed_data.length();
  size_t in_size = str_size;
  std::string output;
  StringOutputStream string_output_stream(&output);
  GzipOutputStream::Options options;
  options.format = GzipOutputStream::GZIP;
  GzipOutputStream compressed_stream(&string_output_stream, options);
  void* out;
  int out_size;
  while (starting_pos < str_size) {
    if (!compressed_stream.Next(&out, &out_size) || out_size <= 0) {
      return absl::InternalError(
          absl::StrCat("An error has occurred during compression: ",
                       compressed_stream.ZlibErrorMessage()));
    }

    if (in_size <= out_size) {
      uncompressed_data.copy(static_cast<char*>(out), in_size, starting_pos);
      // Ensure that the stream's output buffer is truncated to match the total
      // amount of data.
      compressed_stream.BackUp(out_size - static_cast<int>(in_size));
      break;
    }
    uncompressed_data.copy(static_cast<char*>(out), out_size, starting_pos);
    starting_pos += out_size;
    in_size -= out_size;
  }

  if (!compressed_stream.Close()) {
    return absl::InternalError(absl::StrCat(
        "Failed to close the stream: ", compressed_stream.ZlibErrorMessage()));
  }
  return output;
}

absl::StatusOr<absl::Cord> UncompressWithGzip(
    absl::string_view compressed_data) {
  absl::Cord out;
  const void* buffer;
  int size;
  ArrayInputStream sub_stream(compressed_data.data(),
                              static_cast<int>(compressed_data.size()));
  GzipInputStream input_stream(&sub_stream, GzipInputStream::GZIP);

  while (input_stream.Next(&buffer, &size)) {
    if (size <= -1) {
      return absl::InternalError(
          "Uncompress failed: invalid input size returned by the "
          "GzipInputStream.");
    }
    out.Append(absl::string_view(reinterpret_cast<const char*>(buffer), size));
  }

  if (input_stream.ZlibErrorMessage() != nullptr) {
    // Some real error happened during decompression.
    return absl::InternalError(
        absl::StrCat("An error has occurred during decompression:",
                     input_stream.ZlibErrorMessage()));
  }

  return out;
}

}  // namespace fcp

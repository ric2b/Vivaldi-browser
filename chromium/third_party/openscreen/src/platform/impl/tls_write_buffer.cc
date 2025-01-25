// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_write_buffer.h"

#include <algorithm>
#include <cstring>

#include "platform/api/tls_connection.h"
#include "util/osp_logging.h"

namespace openscreen {

TlsWriteBuffer::TlsWriteBuffer() = default;
TlsWriteBuffer::~TlsWriteBuffer() = default;

bool TlsWriteBuffer::Push(ByteView data) {
  const size_t currently_written_bytes =
      bytes_written_so_far_.load(std::memory_order_relaxed);
  const size_t current_read_bytes =
      bytes_read_so_far_.load(std::memory_order_acquire);

  // Calculates the current size of the buffer.
  const size_t bytes_currently_used =
      currently_written_bytes - current_read_bytes;
  OSP_CHECK_LE(bytes_currently_used, kBufferSizeBytes);
  if ((kBufferSizeBytes - bytes_currently_used) < data.size()) {
    return false;
  }

  // Calculates the number of bytes out of `data` to write in the first memcpy
  // operation, which is either all of `data` or the number that can be written
  // before wrapping around to the beginning of the underlying array.
  const size_t current_write_index = currently_written_bytes % kBufferSizeBytes;
  const size_t first_write_len =
      std::min(data.size(), kBufferSizeBytes - current_write_index);
  memcpy(&buffer_[current_write_index], data.data(), first_write_len);

  // If fewer than `data.size()` bytes were transferred in the previous memcpy,
  // copy any remaining bytes to the array, starting at 0 (since the last write
  // must have finished at the end of the array).
  if (first_write_len != data.size()) {
    const uint8_t* new_start =
        static_cast<const uint8_t*>(data.data()) + first_write_len;
    memcpy(buffer_, new_start, data.size() - first_write_len);
  }

  // Store and return updated values.
  const size_t new_write_size = currently_written_bytes + data.size();
  bytes_written_so_far_.store(new_write_size, std::memory_order_release);
  return true;
}

ByteView TlsWriteBuffer::GetReadableRegion() {
  const size_t current_read_bytes =
      bytes_read_so_far_.load(std::memory_order_relaxed);
  const size_t currently_written_bytes =
      bytes_written_so_far_.load(std::memory_order_acquire);

  // Stop reading at either the end of the array or the current write index,
  // whichever is sooner. While there may be more data wrapped around after the
  // end of the array, the API for GetReadableRegion() only guarantees to return
  // a subset of all available read data, so there is no reason to introduce
  // this additional level of complexity.
  const size_t avail = currently_written_bytes - current_read_bytes;
  const size_t begin = current_read_bytes % kBufferSizeBytes;
  const size_t end = std::min(begin + avail, kBufferSizeBytes);
  return ByteView(&buffer_[begin], end - begin);
}

void TlsWriteBuffer::Consume(size_t byte_count) {
  const size_t current_read_bytes =
      bytes_read_so_far_.load(std::memory_order_relaxed);
  const size_t currently_written_bytes =
      bytes_written_so_far_.load(std::memory_order_acquire);

  OSP_CHECK_GE(currently_written_bytes - current_read_bytes, byte_count);
  const size_t new_read_index = current_read_bytes + byte_count;
  bytes_read_so_far_.store(new_read_index, std::memory_order_release);
}

// static
constexpr size_t TlsWriteBuffer::kBufferSizeBytes;

}  // namespace openscreen

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_H_
#define MEDIA_BASE_DECODER_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer_side_data.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"

namespace media {

// A specialized buffer for interfacing with audio / video decoders.
//
// Also includes decoder specific functionality for decryption.
//
// NOTE: It is illegal to call any method when end_of_stream() is true.
class MEDIA_EXPORT DecoderBuffer
    : public base::RefCountedThreadSafe<DecoderBuffer> {
 public:
  // ExternalMemory wraps a class owning a buffer and expose the data interface
  // through Span(). This class is derived by a class that owns the class owning
  // the buffer owner class.
  struct MEDIA_EXPORT ExternalMemory {
   public:
    virtual ~ExternalMemory() = default;
    virtual const base::span<const uint8_t> Span() const = 0;
  };

  using DiscardPadding = std::pair<base::TimeDelta, base::TimeDelta>;

  struct MEDIA_EXPORT TimeInfo {
    TimeInfo();
    ~TimeInfo();
    TimeInfo(const TimeInfo&);
    TimeInfo& operator=(const TimeInfo&);

    // Presentation time of the frame.
    base::TimeDelta timestamp;

    // Presentation duration of the frame.
    base::TimeDelta duration;

    // Duration of (audio) samples from the beginning and end of this frame
    // which should be discarded after decoding. A value of kInfiniteDuration
    // for the first value indicates the entire frame should be discarded; the
    // second value must be base::TimeDelta() in this case.
    DiscardPadding discard_padding;
  };

  // Allocates buffer with |size| > 0. |is_key_frame_| will default to false.
  // If size is 0, no buffer will be allocated.
  explicit DecoderBuffer(size_t size);

  DecoderBuffer(const DecoderBuffer&) = delete;
  DecoderBuffer& operator=(const DecoderBuffer&) = delete;

  // Create a DecoderBuffer whose |data_| is copied from |data|. The buffer's
  // |is_key_frame_| will default to false.
  static scoped_refptr<DecoderBuffer> CopyFrom(base::span<const uint8_t> data);

  // Create a DecoderBuffer where data() of |size| bytes resides within the heap
  // as byte array. The buffer's |is_key_frame_| will default to false.
  //
  // Ownership of |data| is transferred to the buffer.
  static scoped_refptr<DecoderBuffer> FromArray(base::HeapArray<uint8_t> data);

  // Create a DecoderBuffer where data() of |size| bytes resides within the
  // memory referred to by |region| at non-negative offset |offset|. The
  // buffer's |is_key_frame_| will default to false.
  //
  // The shared memory will be mapped read-only.
  //
  // If mapping fails, nullptr will be returned.
  static scoped_refptr<DecoderBuffer> FromSharedMemoryRegion(
      base::UnsafeSharedMemoryRegion region,
      uint64_t offset,
      size_t size);

  // Create a DecoderBuffer where data() of |size| bytes resides within the
  // ReadOnlySharedMemoryRegion referred to by |mapping| at non-negative offset
  // |offset|. The buffer's |is_key_frame_| will default to false.
  //
  // Ownership of |region| is transferred to the buffer.
  static scoped_refptr<DecoderBuffer> FromSharedMemoryRegion(
      base::ReadOnlySharedMemoryRegion region,
      uint64_t offset,
      size_t size);

  // Creates a DecoderBuffer with ExternalMemory. The buffer accessed through
  // the created DecoderBuffer is |span| of |external_memory||.
  // |external_memory| is owned by DecoderBuffer until it is destroyed.
  static scoped_refptr<DecoderBuffer> FromExternalMemory(
      std::unique_ptr<ExternalMemory> external_memory);

  // Create a DecoderBuffer indicating we've reached end of stream.
  //
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<DecoderBuffer> CreateEOSBuffer();

  // Method to verify if subsamples of a DecoderBuffer match.
  static bool DoSubsamplesMatch(const DecoderBuffer& buffer);

  const TimeInfo& time_info() const {
    DCHECK(!end_of_stream());
    return time_info_;
  }

  base::TimeDelta timestamp() const {
    DCHECK(!end_of_stream());
    return time_info_.timestamp;
  }

  // TODO(dalecurtis): This should be renamed at some point, but to avoid a yak
  // shave keep as a virtual with hacker_style() for now.
  virtual void set_timestamp(base::TimeDelta timestamp);

  base::TimeDelta duration() const {
    DCHECK(!end_of_stream());
    return time_info_.duration;
  }

  void set_duration(base::TimeDelta duration) {
    DCHECK(!end_of_stream());
    DCHECK(duration == kNoTimestamp ||
           (duration >= base::TimeDelta() && duration != kInfiniteDuration))
        << duration.InSecondsF();
    time_info_.duration = duration;
  }

  // The pointer to the start of the buffer. Prefer to construct a span around
  // the buffer, such as `base::span(decoder_buffer)`.
  const uint8_t* data() const {
    DCHECK(!end_of_stream());
    if (read_only_mapping_.IsValid())
      return read_only_mapping_.GetMemoryAs<const uint8_t>();
    if (writable_mapping_.IsValid())
      return writable_mapping_.GetMemoryAs<const uint8_t>();
    if (external_memory_)
      return external_memory_->Span().data();
    return data_.data();
  }

  base::span<const uint8_t> AsSpan() const;

  // The number of bytes in the buffer.
  size_t size() const {
    DCHECK(!end_of_stream());
    return size_;
  }

  // Prefer writable_span(), though it should also be removed.
  //
  // TODO(sandersd): Remove writable_data(). https://crbug.com/834088
  uint8_t* writable_data() const {
    DCHECK(!end_of_stream());
    DCHECK(!read_only_mapping_.IsValid());
    DCHECK(!writable_mapping_.IsValid());
    DCHECK(!external_memory_);
    return const_cast<uint8_t*>(data_.data());
  }

  // TODO(sandersd): Remove writable_span(). https://crbug.com/834088
  base::span<uint8_t> writable_span() const {
    // TODO(crbug.com/40284755): `data_` should be converted to HeapArray, then
    // it can give out a span safely.
    return UNSAFE_BUFFERS(base::span(writable_data(), size()));
  }

  inline bool empty() const { return size() == 0u; }

  const DiscardPadding& discard_padding() const {
    DCHECK(!end_of_stream());
    return time_info_.discard_padding;
  }

  void set_discard_padding(const DiscardPadding& discard_padding) {
    DCHECK(!end_of_stream());
    time_info_.discard_padding = discard_padding;
  }

  // Returns DecryptConfig associated with |this|. Returns null if |this| is
  // not encrypted.
  const DecryptConfig* decrypt_config() const {
    DCHECK(!end_of_stream());
    return decrypt_config_.get();
  }

  // TODO(b/331652782): integrate the setter function into the constructor to
  // make |decrypt_config_| immutable.
  void set_decrypt_config(std::unique_ptr<DecryptConfig> decrypt_config) {
    DCHECK(!end_of_stream());
    decrypt_config_ = std::move(decrypt_config);
  }

  bool end_of_stream() const { return is_end_of_stream_; }

  bool is_key_frame() const {
    DCHECK(!end_of_stream());
    return is_key_frame_;
  }

  bool is_encrypted() const {
    DCHECK(!end_of_stream());
    return decrypt_config() && decrypt_config()->encryption_scheme() !=
                                   EncryptionScheme::kUnencrypted;
  }

  void set_is_key_frame(bool is_key_frame) {
    DCHECK(!end_of_stream());
    is_key_frame_ = is_key_frame;
  }

  bool has_side_data() const { return side_data_.has_value(); }
  const std::optional<DecoderBufferSideData>& side_data() const {
    return side_data_;
  }

  // TODO(b/331652782): integrate the setter function into the constructor to
  // make |side_data_| immutable.
  DecoderBufferSideData& WritableSideData();
  void set_side_data(const std::optional<DecoderBufferSideData>& side_data) {
    side_data_ = side_data;
  }

  // Returns true if all fields in |buffer| matches this buffer including
  // |data_|.
  bool MatchesForTesting(const DecoderBuffer& buffer) const;

  // As above, except that |data_| is not compared.
  bool MatchesMetadataForTesting(const DecoderBuffer& buffer) const;

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString(bool verbose = false) const;

  // Returns total memory usage for both bookkeeping and buffered data. The
  // function is added for more accurately memory management.
  virtual size_t GetMemoryUsage() const;

 protected:
  friend class base::RefCountedThreadSafe<DecoderBuffer>;
  enum class DecoderBufferType { kNormal, kEndOfStream };

  // Allocates a buffer with a copy of |data| in it. |is_key_frame_| will
  // default to false.
  explicit DecoderBuffer(base::span<const uint8_t> data);

  DecoderBuffer(base::HeapArray<uint8_t> data);

  DecoderBuffer(base::ReadOnlySharedMemoryMapping mapping, size_t size);

  DecoderBuffer(base::WritableSharedMemoryMapping mapping, size_t size);

  explicit DecoderBuffer(std::unique_ptr<ExternalMemory> external_memory);

  explicit DecoderBuffer(DecoderBufferType decoder_buffer_type);

  virtual ~DecoderBuffer();

  // Encoded data, if it is stored on the heap.
  base::HeapArray<uint8_t> data_;

 private:
  // Constructor helper method for memory allocations.
  void Initialize();

  TimeInfo time_info_;

  // Size of the encoded data.
  size_t size_;

  // Structured side data.
  std::optional<DecoderBufferSideData> side_data_;

  // Encoded data, if it is stored in a read-only shared memory mapping.
  base::ReadOnlySharedMemoryMapping read_only_mapping_;

  // Encoded data, if it is stored in a writable shared memory mapping.
  base::WritableSharedMemoryMapping writable_mapping_;

  std::unique_ptr<ExternalMemory> external_memory_;

  // Encryption parameters for the encoded data.
  std::unique_ptr<DecryptConfig> decrypt_config_;

  // Whether the frame was marked as a keyframe in the container.
  bool is_key_frame_ = false;

  // Whether the buffer represent the end of stream.
  const bool is_end_of_stream_ = false;
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_MESSAGE_DEMUXER_H_
#define OSP_PUBLIC_MESSAGE_DEMUXER_H_

#include <map>
#include <memory>
#include <vector>

#include "osp/msgs/osp_messages.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/span.h"

namespace openscreen::osp {

// This class separates QUIC stream data into CBOR messages by reading a type
// prefix from the stream and passes those messages to any callback matching the
// source endpoint and message type.  If there is no callback for a given
// message type, it will also try a default message listener.
class MessageDemuxer {
 public:
  class MessageCallback {
   public:
    MessageCallback();
    MessageCallback(const MessageCallback&) = delete;
    MessageCallback& operator=(const MessageCallback&) = delete;
    MessageCallback(MessageCallback&&) noexcept = default;
    MessageCallback& operator=(MessageCallback&&) noexcept = default;
    virtual ~MessageCallback();

    // `buffer` contains data for a message of type `message_type`.  However,
    // the data may be incomplete, in which case the callback should return an
    // error code of Error::Code::kCborIncompleteMessage.  This way,
    // the MessageDemuxer knows to neither consume the data nor discard it as
    // bad.
    virtual ErrorOr<size_t> OnStreamMessage(uint64_t instance_id,
                                            uint64_t connection_id,
                                            msgs::Type message_type,
                                            const uint8_t* buffer,
                                            size_t buffer_size,
                                            Clock::time_point now) = 0;
  };

  class MessageWatch {
   public:
    MessageWatch();
    MessageWatch(MessageDemuxer* parent,
                 bool is_default,
                 uint64_t instance_id,
                 msgs::Type message_type);
    MessageWatch(const MessageWatch&) = delete;
    MessageWatch& operator=(const MessageWatch&) = delete;
    MessageWatch(MessageWatch&&) noexcept;
    MessageWatch& operator=(MessageWatch&&) noexcept;
    ~MessageWatch();

    explicit operator bool() const { return parent_; }
    // Stop this MessageWatch by calling `StopWatching()` and reset its members.
    void Reset();

   private:
    // Stop this MessageWatch if `parent_` is not empty. Otherwise, this is a
    // no-op.
    void StopWatching();

    MessageDemuxer* parent_ = nullptr;
    bool is_default_ = false;
    uint64_t instance_id_ = 0u;
    msgs::Type message_type_ = msgs::Type::kUnknown;
  };

  static constexpr size_t kDefaultBufferLimit = 1 << 16;

  MessageDemuxer(ClockNowFunctionPtr now_function, size_t buffer_limit);
  MessageDemuxer(const MessageDemuxer&) = delete;
  MessageDemuxer& operator=(const MessageDemuxer&) = delete;
  MessageDemuxer(MessageDemuxer&&) noexcept = delete;
  MessageDemuxer& operator=(MessageDemuxer&&) noexcept = delete;
  ~MessageDemuxer();

  // Starts watching for messages of type `message_type` from the instance
  // identified by `instance_id`.  When such a message arrives, or if some
  // are already buffered, `callback` will be called with the message data.
  MessageWatch WatchMessageType(uint64_t instance_id,
                                msgs::Type message_type,
                                MessageCallback* callback);

  // Starts watching for messages of type `message_type` from any instance when
  // there is not callback set for its specific instance ID.
  MessageWatch SetDefaultMessageTypeWatch(msgs::Type message_type,
                                          MessageCallback* callback);

  // Gives data from `instance_id` to the demuxer for processing.
  void OnStreamData(uint64_t instance_id,
                    uint64_t connection_id,
                    const uint8_t* data,
                    size_t data_size);

  // Clears the buffered data when the stream is closed.
  void OnStreamClose(uint64_t instance_id, uint64_t connection_id);

 private:
  struct HandleStreamBufferResult {
    bool handled;
    size_t consumed;
  };

  void StopWatchingMessageType(uint64_t instance_id, msgs::Type message_type);
  void StopDefaultMessageTypeWatch(msgs::Type message_type);

  HandleStreamBufferResult HandleStreamBufferLoop(
      uint64_t instance_id,
      uint64_t connection_id,
      std::map<uint64_t, std::map<msgs::Type, MessageCallback*>>::iterator
          instance_entry,
      std::vector<uint8_t>& buffer);

  HandleStreamBufferResult HandleStreamBuffer(
      uint64_t instance_id,
      uint64_t connection_id,
      std::map<msgs::Type, MessageCallback*>* message_callbacks,
      std::vector<uint8_t>& buffer);

  const ClockNowFunctionPtr now_function_;
  const size_t buffer_limit_;
  std::map<uint64_t, std::map<msgs::Type, MessageCallback*>> message_callbacks_;
  std::map<msgs::Type, MessageCallback*> default_callbacks_;

  // Map<instance_id, Map<connection_id, data_buffer>>
  std::map<uint64_t, std::map<uint64_t, std::vector<uint8_t>>> buffers_;
};

class MessageTypeDecoder {
 public:
  static ErrorOr<msgs::Type> DecodeType(ByteView buffer,
                                        size_t* num_bytes_decoded);

 private:
  static ErrorOr<uint64_t> DecodeVarUint(ByteView buffer,
                                         size_t* num_bytes_decoded);
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_MESSAGE_DEMUXER_H_

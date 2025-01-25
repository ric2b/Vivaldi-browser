// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_INVALIDATIONS_STOMP_FRAME_BUILDER_H_
#define SYNC_INVALIDATIONS_STOMP_FRAME_BUILDER_H_

#include <map>
#include <string>
#include <string_view>

namespace vivaldi {
namespace stomp {
class StompFrameBuilder {
 public:
  StompFrameBuilder();
  ~StompFrameBuilder();

  StompFrameBuilder(const StompFrameBuilder&) = delete;
  StompFrameBuilder& operator=(const StompFrameBuilder&) = delete;

  bool ProcessIncoming(std::string_view incoming);
  std::unique_ptr<StompFrameBuilder> TakeOverNextframe() {
    return std::move(next_frame_);
  }
  bool IsComplete() { return frame_state_ == FrameState::kFrameComplete; }

  std::string_view command() { return command_; }
  const std::map<std::string_view, std::string_view>& headers() {
    return headers_;
  }
  std::string_view body() { return body_; }

 private:
  StompFrameBuilder(const std::string& line_ending,
                    const std::string& header_ending);

  enum class FrameState { kReceivingHeader, kReceivingBody, kFrameComplete };

  bool ProcessIncomingHeaders(std::string_view incoming);
  bool ProcessIncomingBody(std::string_view incoming);
  bool OnHeadersRead();

  FrameState frame_state_ = FrameState::kReceivingHeader;
  std::string header_string_;
  std::string body_;
  size_t remaining_body_size_ = 0;

  // The STOMP specification does not mandate the line endings to be consistent,
  // but it makes sense to assume they are for a given connection.
  std::string line_ending_;
  std::string header_ending_;

  std::unique_ptr<StompFrameBuilder> next_frame_;

  std::string_view command_;
  std::map<std::string_view, std::string_view> headers_;
};
}  // namespace stomp
}  // namespace vivaldi
#endif  // SYNC_INVALIDATIONS_STOMP_FRAME_BUILDER_H_
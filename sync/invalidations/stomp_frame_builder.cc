// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "sync/invalidations/stomp_frame_builder.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "sync/invalidations/stomp_constants.h"

namespace vivaldi {
namespace stomp {
namespace {
// Invalidation frames are unlikely to exceed 1KiB in size, with current server
// implementation. Accept bodies up to 2KiB to be safe.
constexpr size_t kMaxBodySize = 1 << 11;

// Invalidation frames are unlikely to exceed 1KiB in size, with current server
// implementation. Accept headers up to 2KiB to be safe.
constexpr size_t kMaxHeaderSize = 1 << 11;

}  // namespace

StompFrameBuilder::StompFrameBuilder() : StompFrameBuilder("", "") {}

StompFrameBuilder::StompFrameBuilder(const std::string& line_ending,
                                     const std::string& header_ending)
    : line_ending_(line_ending), header_ending_(header_ending) {
  header_string_.reserve(kMaxHeaderSize);
  body_.reserve(kMaxBodySize);
}

StompFrameBuilder::~StompFrameBuilder() = default;

bool StompFrameBuilder::ProcessIncoming(std::string_view incoming) {
  switch (frame_state_) {
    case FrameState::kReceivingHeader:
      return ProcessIncomingHeaders(incoming);
    case FrameState::kReceivingBody:
      return ProcessIncomingBody(incoming);
    case FrameState::kFrameComplete:
      NOTREACHED();
  }
}

bool StompFrameBuilder::ProcessIncomingHeaders(std::string_view incoming) {
  size_t header_end;
  if (line_ending_.empty()) {
    // This is the first frame for this connection. Read it until we know what
    // line ending is used.
    header_end = incoming.find(kLf);
    if (header_end == std::string_view::npos) {
      header_string_.append(incoming);
      return true;
    }

    header_string_.append(incoming.substr(0, header_end + 1));
    incoming.remove_prefix(header_end + 1);
    if (header_string_.ends_with(kCrLf)) {
      line_ending_ = kCrLf;
    } else {
      line_ending_ = {kLf};
    }
    header_ending_ = line_ending_ + line_ending_;
  }

  CHECK(!line_ending_.empty());

  if (headers_.empty()) {
    // Discard heart beats
    while (incoming.starts_with(line_ending_)) {
      incoming.remove_prefix(line_ending_.size());
    }
  }

  while ((header_end = incoming.find(kLf)) != std::string_view::npos) {
    header_string_.append(incoming.substr(0, header_end + 1));
    incoming.remove_prefix(header_end + 1);

    if (header_string_.size() > kMaxHeaderSize) {
      return false;
    }

    if (header_string_.ends_with(header_ending_)) {
      frame_state_ = FrameState::kReceivingBody;
      if (!OnHeadersRead()) {
        return false;
      }
      if (!incoming.empty()) {
        return ProcessIncomingBody(incoming);
      }
      return true;
    }
  }
  header_string_.append(incoming);
  return true;
}

bool StompFrameBuilder::ProcessIncomingBody(std::string_view incoming) {
  if (remaining_body_size_ == std::string::npos) {
    // Body goes until the first NUL byte;
    size_t nul_position = incoming.find(kNul);
    body_.append(incoming.substr(0, nul_position));
    if (nul_position == std::string_view::npos) {
      return true;
    }
    if (body_.size() > kMaxBodySize) {
      return false;
    }
    incoming.remove_prefix(nul_position + 1);
  } else {
    if (remaining_body_size_ >= incoming.size()) {
      body_.append(incoming);
      remaining_body_size_ -= incoming.size();
      return true;
    }

    body_.append(incoming.substr(0, remaining_body_size_));
    incoming.remove_prefix(remaining_body_size_);
    if (incoming.front() != kNul) {
      return false;
    }
    incoming.remove_prefix(1);
  }

  remaining_body_size_ = 0;
  frame_state_ = FrameState::kFrameComplete;

  next_frame_.reset(new StompFrameBuilder(line_ending_, header_ending_));
  if (!incoming.empty()) {
    next_frame_->ProcessIncoming(incoming);
  }

  return true;
}

bool StompFrameBuilder::OnHeadersRead() {
  std::vector<std::string_view> header_lines = SplitStringPieceUsingSubstr(
      header_string_, line_ending_, base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);

  if (header_lines.empty())
    return false;

  command_ = header_lines[0];
  header_lines.erase(header_lines.begin());

  for (std::string_view header_line : header_lines) {
    size_t separator = header_line.find(':');
    if (separator == std::string_view::npos)
      return false;
    headers_[header_line.substr(0, separator)] =
        header_line.substr(separator + 1);
  }

  // If this fails, that means we accidentally got some heartbeats included in
  // the headers_string as part of the line ending detection.
  CHECK(!command_.empty());

  if (command_ != kErrorCommand && command_ != kMessageCommand) {
    CHECK(remaining_body_size_ == 0);
    return true;
  }

  const auto& content_length_header = headers_.find(kContentLengthHeader);
  if (content_length_header == headers_.end()) {
    remaining_body_size_ = std::string::npos;
  } else {
    if (!base::StringToSizeT(content_length_header->second,
                             &remaining_body_size_) ||
        remaining_body_size_ > kMaxBodySize)
      return false;
  }

  return true;
}
}  // namespace stomp
}  // namespace vivaldi
// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/presentation/presentation_common.h"

#include <utility>

#include "util/stringutil.h"

namespace openscreen::osp {

std::unique_ptr<ProtocolConnection> GetProtocolConnection(
    uint64_t instance_id) {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionServer()
      ->CreateProtocolConnection(instance_id);
}

MessageDemuxer& GetServerDemuxer() {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionServer()
      ->GetMessageDemuxer();
}

MessageDemuxer& GetClientDemuxer() {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionClient()
      ->GetMessageDemuxer();
}

PresentationID::PresentationID(std::string presentation_id)
    : id_(Error::Code::kParseError) {
  // The spec dictates that the presentation ID must be composed
  // of at least 16 ASCII characters.
  bool is_valid = presentation_id.length() >= 16;
  for (const char& c : presentation_id) {
    is_valid &= stringutil::ascii_isprint(c);
  }

  if (is_valid) {
    id_ = std::move(presentation_id);
  }
}

}  // namespace openscreen::osp

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/presentation/presentation_id.h"

#include <utility>

#include "util/string_util.h"

namespace openscreen::osp {

PresentationID::PresentationID(std::string presentation_id)
    : id_(Error::Code::kParseError) {
  // The spec dictates that the presentation ID must be composed
  // of at least 16 ASCII characters.
  bool is_valid = presentation_id.length() >= 16;
  for (const char& c : presentation_id) {
    is_valid &= string_util::ascii_isprint(c);
  }

  if (is_valid) {
    id_ = std::move(presentation_id);
  }
}

PresentationID::~PresentationID() = default;

}  // namespace openscreen::osp

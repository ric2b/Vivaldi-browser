// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_PRESENTATION_PRESENTATION_ID_H_
#define OSP_IMPL_PRESENTATION_PRESENTATION_ID_H_

#include <string>

#include "platform/base/error.h"

namespace openscreen::osp {

class PresentationID {
 public:
  explicit PresentationID(const std::string presentation_id);
  PresentationID(const PresentationID&) = delete;
  PresentationID& operator=(const PresentationID&) = delete;
  PresentationID(PresentationID&&) noexcept = delete;
  PresentationID& operator=(PresentationID&&) noexcept = delete;
  ~PresentationID();

  operator bool() { return id_; }
  operator std::string() { return id_.value(); }

 private:
  ErrorOr<std::string> id_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_PRESENTATION_PRESENTATION_ID_H_

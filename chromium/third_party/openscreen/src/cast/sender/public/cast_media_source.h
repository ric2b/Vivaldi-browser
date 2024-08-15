// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_PUBLIC_CAST_MEDIA_SOURCE_H_
#define CAST_SENDER_PUBLIC_CAST_MEDIA_SOURCE_H_

#include <string>
#include <vector>

#include "platform/base/error.h"

namespace openscreen::cast {

class CastMediaSource {
 public:
  static ErrorOr<CastMediaSource> From(const std::string& source);

  CastMediaSource(std::string source, std::vector<std::string> app_ids);
  CastMediaSource(const CastMediaSource& other);
  CastMediaSource(CastMediaSource&& other) noexcept;
  ~CastMediaSource();

  CastMediaSource& operator=(const CastMediaSource& other);
  CastMediaSource& operator=(CastMediaSource&& other) noexcept;

  bool ContainsAppId(const std::string& app_id) const;
  bool ContainsAnyAppIdFrom(const std::vector<std::string>& app_ids) const;

  const std::string& source_id() const { return source_id_; }
  const std::vector<std::string>& app_ids() const { return app_ids_; }

 private:
  std::string source_id_;
  std::vector<std::string> app_ids_;
};

}  // namespace openscreen::cast

#endif  // CAST_SENDER_PUBLIC_CAST_MEDIA_SOURCE_H_

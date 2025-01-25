// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/url.h"

#include <limits.h>

#include <string_view>
#include <utility>

#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace openscreen {

namespace {

// Given a string and a range inside the string, compares it to the given
// lower-case `compare_to` buffer.
bool CompareSchemeComponent(std::string_view spec,
                            const url::Component& component,
                            std::string_view compare_to) {
  if (component.is_empty()) {
    return compare_to.empty();  // When component is empty, match empty scheme.
  }
  for (int i = 0; i < component.len; ++i) {
    if (tolower(spec[i]) != compare_to[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

Url::Url(const std::string& source) {
  url::Parsed parsed;
  url::Component scheme;
  const char* url = source.c_str();
  size_t length = source.size();
  if (length > INT_MAX) {
    return;
  }
  int url_length = static_cast<int>(length);

  if (!url::ExtractScheme(url, url_length, &scheme)) {
    return;
  }

  if (CompareSchemeComponent(url, scheme, url::kFileScheme) ||
      CompareSchemeComponent(url, scheme, url::kFileSystemScheme) ||
      CompareSchemeComponent(url, scheme, url::kMailToScheme)) {
    // NOTE: Special schemes that are unsupported.
    return;
  } else if (url::IsStandard(url, scheme)) {
    url::ParseStandardURL(url, url_length, &parsed);
    if (!parsed.host.is_valid()) {
      return;
    }
  } else {
    url::ParsePathURL(url, url_length, true, &parsed);
  }

  if (!parsed.scheme.is_nonempty()) {
    return;
  }
  scheme_ = std::string(url + parsed.scheme.begin, url + parsed.scheme.end());

  if (parsed.host.is_valid()) {
    has_host_ = true;
    host_ = std::string(url + parsed.host.begin, url + parsed.host.end());
  }

  if (parsed.port.is_nonempty()) {
    int parse_result = url::ParsePort(url, parsed.port);
    if (parse_result == url::PORT_INVALID) {
      return;
    } else if (parse_result >= 0) {
      has_port_ = true;
      port_ = parse_result;
    }
  }

  if (parsed.path.is_nonempty()) {
    has_path_ = true;
    path_ = std::string(url + parsed.path.begin, url + parsed.path.end());
  }

  if (parsed.query.is_nonempty()) {
    has_query_ = true;
    query_ = std::string(url + parsed.query.begin, url + parsed.query.end());
  }

  is_valid_ = true;
}

Url::Url(const Url&) = default;

Url::Url(Url&& other) noexcept
    : is_valid_(other.is_valid_),
      has_host_(other.has_host_),
      has_port_(other.has_port_),
      has_path_(other.has_path_),
      has_query_(other.has_query_),
      scheme_(std::move(other.scheme_)),
      host_(std::move(other.host_)),
      port_(other.port_),
      path_(std::move(other.path_)),
      query_(std::move(other.query_)) {
  other.is_valid_ = false;
}

Url::~Url() = default;

Url& Url::operator=(const Url&) = default;

Url& Url::operator=(Url&& other) {
  is_valid_ = other.is_valid_;
  has_host_ = other.has_host_;
  has_port_ = other.has_port_;
  has_path_ = other.has_path_;
  has_query_ = other.has_query_;
  scheme_ = std::move(other.scheme_);
  host_ = std::move(other.host_);
  port_ = other.port_;
  path_ = std::move(other.path_);
  query_ = std::move(other.query_);
  other.is_valid_ = false;
  return *this;
}

}  // namespace openscreen

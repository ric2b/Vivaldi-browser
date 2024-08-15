// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/std_util.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "util/osp_logging.h"

namespace openscreen {

std::string& RemoveWhitespace(std::string& s) {
  s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
  return s;
}

}  // namespace openscreen

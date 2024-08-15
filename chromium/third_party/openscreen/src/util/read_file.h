// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_READ_FILE_H_
#define UTIL_READ_FILE_H_

#include <string>
#include <string_view>

namespace openscreen {

std::string ReadEntireFileToString(std::string_view filename);

}  // namespace openscreen

#endif  // UTIL_READ_FILE_H_

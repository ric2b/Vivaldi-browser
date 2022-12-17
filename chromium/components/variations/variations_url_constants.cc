// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_url_constants.h"

#include "app/vivaldi_constants.h"

namespace variations {

// Default server of Variations seed info.
const char kDefaultServerUrl[] =
    KNOWN_404("/chrome-variations/seed");

const char kDefaultInsecureServerUrl[] =
    KNOWN_404("/chrome-variations/seed");

}  // namespace variations

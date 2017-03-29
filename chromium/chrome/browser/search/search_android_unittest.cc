// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/common/chrome_switches.h"
#include "components/search/search.h"
#include "components/search/search_switches.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

namespace {

TEST(SearchTest, QueryExtractionEnabled) {
  // Query extraction is always enabled on mobile.
  EXPECT_FALSE(IsQueryExtractionEnabled());
}

}  // namespace

}  // namespace chrome

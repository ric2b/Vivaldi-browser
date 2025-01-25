// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/large_icon_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "components/search_engines/search_engines_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

class LargeIconServiceFactoryTest : public testing::Test {};

TEST_F(LargeIconServiceFactoryTest, LargeFaviconFromGoogleDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(switches::kSearchEngineChoiceTrigger);

#if BUILDFLAG(IS_ANDROID)
  const int expected = 24;
#else
  const int expected = 16;
#endif

  EXPECT_EQ(LargeIconServiceFactory::desired_size_in_dip_for_server_requests(),
            expected);
}

TEST_F(LargeIconServiceFactoryTest, SearchEngineChoiceEnabled) {
  base::test::ScopedFeatureList features{switches::kSearchEngineChoiceTrigger};

#if BUILDFLAG(IS_ANDROID)
  const int expected = 32;
#else
  const int expected = 16;
#endif

  EXPECT_EQ(LargeIconServiceFactory::desired_size_in_dip_for_server_requests(),
            expected);
}

// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ADVERSE_AD_FILTER_TEST_HARNESS_H_
#define ADVERSE_AD_FILTER_TEST_HARNESS_H_

#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {
class SubresourceFilterContentSettingsManager;
}

// Minimal testharness for Vivaldis adverse filter functionality.
class AdverseAdFilterTestHarness : public ChromeRenderViewHostTestHarness {
 public:
  // Allowlist rules must prefix a disallowed rule in order to work correctly.
  static constexpr const char kDefaultAllowedSuffix[] = "not_disallowed.html";
  static constexpr const char kDefaultDisallowedSuffix[] = "disallowed.html";
  static constexpr const char kDefaultDisallowedUrl[] =
      "https://example.test/disallowed.html";

  AdverseAdFilterTestHarness();
  ~AdverseAdFilterTestHarness() override;
  AdverseAdFilterTestHarness(const AdverseAdFilterTestHarness&) = delete;
  AdverseAdFilterTestHarness& operator=(const AdverseAdFilterTestHarness&) =
      delete;

  // ChromeRenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;

  // Returns the frame host the navigation commit in, or nullptr if it did not
  // succeed.
  content::RenderFrameHost* SimulateNavigateAndCommit(
      const GURL& url,
      content::RenderFrameHost* rfh);

  // Creates a subframe as a child of |parent|, and navigates it to a URL
  // disallowed by the default ruleset (kDefaultDisallowedUrl). Returns the
  // frame host the navigation commit in, or nullptr if it did not succeed.
  content::RenderFrameHost* CreateAndNavigateDisallowedSubframe(
      content::RenderFrameHost* parent);

 private:
  base::ScopedTempDir ruleset_service_dir_;
  TestingPrefServiceSimple pref_service_;
  subresource_filter::testing::ScopedSubresourceFilterConfigurator
      scoped_configuration_;

  // scoped_refptr<FakeSafeBrowsingDatabaseManager>
  // fake_safe_browsing_database_;
};

#endif  // ADVERSE_AD_FILTER_TEST_HARNESS_H_

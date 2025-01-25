// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/features/vivaldi_features.h"

#import "app/vivaldi_version_info.h"

BASE_FEATURE(kShowTopSites,
             "kShowTopSites",
             base::FEATURE_ENABLED_BY_DEFAULT
);

bool IsTopSitesEnabled() {
  return base::FeatureList::IsEnabled(kShowTopSites);
}

BASE_FEATURE(kViewMarkdownAsHTML,
             "kViewMarkdownAsHTML",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsViewMarkdownAsHTMLEnabled() {
  return base::FeatureList::IsEnabled(kViewMarkdownAsHTML);
}

BASE_FEATURE(kShowNewSpeedDialDialog,
             "kShowNewSpeedDialDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNewSpeedDialDialogEnabled() {
  return base::FeatureList::IsEnabled(kShowNewSpeedDialDialog);
}

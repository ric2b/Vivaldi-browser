// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/features/vivaldi_features.h"

#import "app/vivaldi_version_info.h"

BASE_FEATURE(kNewStartPage,
             "kNewStartPage",
#if (BUILD_VERSION(VIVALDI_RELEASE) == RELEASE_TYPE_ID_vivaldi_snapshot \
    || BUILD_VERSION(VIVALDI_RELEASE) == RELEASE_TYPE_ID_vivaldi_sopranos)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsNewStartPageIsEnabled() {
  return base::FeatureList::IsEnabled(kNewStartPage);
}

BASE_FEATURE(kViewMarkdownAsHTML,
             "kViewMarkdownAsHTML",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsViewMarkdownAsHTMLEnabled() {
  return base::FeatureList::IsEnabled(kViewMarkdownAsHTML);
}

// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiSearchEngineChangePromptMediator () {
  const TemplateURL* _recommendedProvider;  // weak
  const TemplateURL* _currentProvider;  // weak
}
@end

@implementation VivaldiSearchEngineChangePromptMediator

- (instancetype)
    initWithRecommendedProvider:(const TemplateURL*)recommendedProvider
                currentProvider:(const TemplateURL*)currentProvider {
  self = [super init];
  if (self) {
    _recommendedProvider = recommendedProvider;
    _currentProvider = currentProvider;
  }
  return self;
}

- (void)disconnect {
  _recommendedProvider = nullptr;
  _currentProvider = nullptr;
  _consumer = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiSearchEngineChangePromptViewConsumer>)consumer {
  _consumer = consumer;

  NSString* recommendedProvider =
      base::SysUTF16ToNSString(_recommendedProvider->short_name());
  NSString* currentProvider =
      base::SysUTF16ToNSString(_currentProvider->short_name());

  const std::u16string& recommendedProviderStdString =
      base::SysNSStringToUTF16(recommendedProvider);

  [self.consumer setRecommendedProvider:recommendedProvider];

  NSString* description;

  // When recommended search engine is Startpage we have a different string.
  // Since it is limited to Startpage it is fine to use hardcoded string here.
  if ([[recommendedProvider lowercaseString] isEqualToString:@"startpage"] ||
      [[recommendedProvider lowercaseString] isEqualToString:@"startpage.com"])
  {
    description =
        l10n_util::GetNSStringF(
            IDS_IOS_SE_CHANGE_NEW_SEARCH_ENGINE_STARTPAGE_DESCRIPTION,
                recommendedProviderStdString, recommendedProviderStdString);
  } else {
    description =
        l10n_util::GetNSStringF(IDS_IOS_SE_CHANGE_NEW_SEARCH_ENGINE_DESCRIPTION,
                                recommendedProviderStdString);
  }

  NSString* useRecommendedProviderTitle =
      l10n_util::GetNSStringF(IDS_IOS_SE_CHANGE_USE_NEW_ENGINE_TITLE,
                              recommendedProviderStdString);
  NSString* useCurrentProviderTitle =
      l10n_util::GetNSStringF(IDS_IOS_SE_CHANGE_USE_OLD_ENGINE_TITLE,
                              base::SysNSStringToUTF16(currentProvider));
  [self.consumer
      setDescription:description
          recommendedProviderTitle:useRecommendedProviderTitle
              currentProviderTitle:useCurrentProviderTitle];
}

@end

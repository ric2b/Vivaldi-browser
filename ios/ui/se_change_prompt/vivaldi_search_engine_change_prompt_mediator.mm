// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "ios/ui/se_change_prompt/search_engine_change_prompt_swift.h"

@interface VivaldiSearchEngineChangePromptMediator () {
  std::vector<TemplateURL*> _providers;  // weak
}

@property(nonatomic, assign) VivaldiSearchEngineChangePromptType promptType;
@end

@implementation VivaldiSearchEngineChangePromptMediator

- (instancetype)initWithProviders:(const std::vector<TemplateURL*>)providers
                       promptType:
                           (VivaldiSearchEngineChangePromptType)promptType {
  self = [super init];
  if (self) {
    _providers = providers;
    _promptType = promptType;
  }
  return self;
}

- (void)disconnect {
  _providers.clear();
  _consumer = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiSearchEngineChangePromptViewConsumer>)consumer {
  _consumer = consumer;

  NSMutableArray<VivaldiSearchEngineChangePromptPartnerItem*>* partners =
      [NSMutableArray array];
  for (TemplateURL* provider : _providers) {
    if (provider) {
      TemplateURLID partnerId = provider->id();
      NSString* shortName = base::SysUTF16ToNSString(provider->short_name());
      VivaldiSearchEngineChangePromptPartnerItem* partnerItem =
          [[VivaldiSearchEngineChangePromptPartnerItem alloc]
              initWithPartnerId:partnerId
                      shortName:shortName];
      [partners addObject:partnerItem];
    }
  }

  [self.consumer setRecommendedProviders:[partners copy]
                              promptType:_promptType];
}

@end

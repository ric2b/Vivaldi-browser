// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ad_blocker/adblock_content_rule_list_provider.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace adblock_filter {

NSString* const kTestListOne = @R"===([
  {
    "trigger": {
      "url-filter": "example.com",
      "resource-type": ["image"],
    },
    "action": {
      "type": "ignore-previous-rules"
    }
  }
])===";

AdBlockerContentRuleListProvider::AdBlockerContentRuleListProvider()
    : weak_ptr_factory_(this) {
  base::WeakPtr<AdBlockerContentRuleListProvider> weak_this =
      weak_ptr_factory_.GetWeakPtr();

  [WKContentRuleListStore.defaultStore
      compileContentRuleListForIdentifier:@"test_list_1"
                  encodedContentRuleList:kTestListOne
                        completionHandler:^(WKContentRuleList* rule_list,
                                            NSError* error) {
                          if (!weak_this.get()) {
                            return;
                          }
                          if (!rule_list) {
                            DLOG(INFO) << "Failed loading rule list 1";
                            return;
                          }
                          rule_lists_[1]=rule_list;
                          InstallContentRuleLists();
                        }];
}

AdBlockerContentRuleListProvider::~AdBlockerContentRuleListProvider() {}

void AdBlockerContentRuleListProvider::InstallContentRuleLists() {
  for (auto [id, rule_list] : rule_lists_ ) {
    [user_content_controller_ removeContentRuleList:rule_list];
    [user_content_controller_ addContentRuleList:rule_list];
  }
  DLOG(INFO) << "Rules lists:" << rule_lists_.size();
}

void AdBlockerContentRuleListProvider::UninstallContentRuleLists() {
}

void AdBlockerContentRuleListProvider::SetUserContentController(
    WKUserContentController* user_content_controller) {
  user_content_controller_ = user_content_controller;
  InstallContentRuleLists();
}

}  // namespace adblock_filter

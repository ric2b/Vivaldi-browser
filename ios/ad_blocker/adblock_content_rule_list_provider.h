// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_AD_BLOCKER_ADBLOCK_CONTENT_RULE_LIST_PROVIDER_H_
#define IOS_AD_BLOCKER_ADBLOCK_CONTENT_RULE_LIST_PROVIDER_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

@class WKContentRuleList;
@class WKUserContentController;

namespace adblock_filter {

// A provider class that handles compiling and configuring Content Blocker
// rules.
class AdBlockerContentRuleListProvider {
 public:
  explicit AdBlockerContentRuleListProvider();
  ~AdBlockerContentRuleListProvider();

  // Sets the WKUserContentController that this provider will install its rules
  // on.
  void SetUserContentController(
      WKUserContentController* user_content_controller);

 private:
  AdBlockerContentRuleListProvider(const AdBlockerContentRuleListProvider&) =
      delete;
  AdBlockerContentRuleListProvider& operator=(
      const AdBlockerContentRuleListProvider&) = delete;

  // Installs the content rule list that should be installed given the current
  // ad blocker setting.
  void InstallContentRuleLists();

  // Uninstalls all content rule lists installed by this provider.
  void UninstallContentRuleLists();

  __weak WKUserContentController* user_content_controller_;
  std::map<uint32_t, WKContentRuleList*> rule_lists_;

  base::OnceCallback<void(bool)> update_callback_;

  base::WeakPtrFactory<AdBlockerContentRuleListProvider> weak_ptr_factory_;
};

}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_ADBLOCK_CONTENT_RULE_LIST_PROVIDER_H_

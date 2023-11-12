// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ad_blocker/adblock_content_rule_list_provider.h"

#import <vector>

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/barrier_callback.h"
#import "base/containers/cxx20_erase.h"
#import "base/json/json_string_value_serializer.h"
#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "ios/ad_blocker/utils.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace adblock_filter {

namespace {

constexpr char kListNamePrefix[] = "Vivaldi_";
constexpr char kTrackerListNamePrefix[] = "Trackers_";
constexpr char kAdsListNamePrefix[] = "Ads_";
constexpr base::StringPiece kListNameGroupPrefix[2] = {kTrackerListNamePrefix,
                                                       kAdsListNamePrefix};

class AdBlockerContentRuleListProviderImpl
    : public AdBlockerContentRuleListProvider,
      public web::WKWebViewConfigurationProviderObserver {
 public:
  explicit AdBlockerContentRuleListProviderImpl(
      web::BrowserState* browser_state,
      RuleGroup group,
      base::RepeatingClosure rules_applied);
  ~AdBlockerContentRuleListProviderImpl() override;
  AdBlockerContentRuleListProviderImpl(
      const AdBlockerContentRuleListProviderImpl&) = delete;
  AdBlockerContentRuleListProviderImpl& operator=(
      const AdBlockerContentRuleListProviderImpl&) = delete;

  // Implementing AdBlockerContentRuleListProvider
  void InstallContentRuleLists(const base::Value::List& lists) override;

 private:
  // Implementing WKWebViewConfigurationProviderObserver
  void DidCreateNewConfiguration(
      web::WKWebViewConfigurationProvider* config_provider,
      WKWebViewConfiguration* new_config) override;

  void CompileContentRuleLists();
  void RemoveInstalledLists();
  void DoInstallContentRuleLists(
      std::vector<WKContentRuleList*> content_rule_lists);
  void ApplyContentRuleLists();

  web::BrowserState* browser_state_;
  RuleGroup group_;
  __weak WKUserContentController* user_content_controller_;
  int next_list_number_{0};
  std::vector<WKContentRuleList*> installed_content_rule_lists_;
  base::RepeatingClosure on_rules_applied_;

  base::WeakPtrFactory<AdBlockerContentRuleListProviderImpl> weak_ptr_factory_{
      this};
};

AdBlockerContentRuleListProviderImpl::AdBlockerContentRuleListProviderImpl(
    web::BrowserState* browser_state,
    RuleGroup group,
    base::RepeatingClosure on_rules_applied)
    : browser_state_(browser_state),
      group_(group),
      on_rules_applied_(std::move(on_rules_applied)) {
  web::WKWebViewConfigurationProvider& config_provider =
      web::WKWebViewConfigurationProvider::FromBrowserState(browser_state_);
  DidCreateNewConfiguration(&config_provider,
                            config_provider.GetWebViewConfiguration());
  config_provider.AddObserver(this);
}

AdBlockerContentRuleListProviderImpl::~AdBlockerContentRuleListProviderImpl() {
  web::WKWebViewConfigurationProvider::FromBrowserState(browser_state_)
      .RemoveObserver(this);
}

void AdBlockerContentRuleListProviderImpl::InstallContentRuleLists(
    const base::Value::List& lists) {
  if (lists.empty()) {
    RemoveInstalledLists();
    on_rules_applied_.Run();
    return;
  }

  __block auto compile_and_apply = base::BarrierCallback<WKContentRuleList*>(
      lists.size(),
      base::BindOnce(
          &AdBlockerContentRuleListProviderImpl::DoInstallContentRuleLists,
          weak_ptr_factory_.GetWeakPtr()));

  for (const auto& list : lists) {
    DCHECK(list.is_string());
    std::string list_name(kListNamePrefix);
    list_name.append(
        std::string(kListNameGroupPrefix[static_cast<size_t>(group_)]));
    list_name.append(base::NumberToString(next_list_number_++));

    [WKContentRuleListStore.defaultStore
        compileContentRuleListForIdentifier:
            [NSString stringWithUTF8String:list_name.c_str()]
                     encodedContentRuleList:
                         [NSString
                             stringWithUTF8String:list.GetString().c_str()]
                          completionHandler:^(WKContentRuleList* rule_list,
                                              NSError* error) {
                            if (!rule_list) {
                              DLOG(INFO) << "Failed loading rule list";
                            }
                            compile_and_apply.Run(rule_list);
                          }];
  }
}

void AdBlockerContentRuleListProviderImpl::DidCreateNewConfiguration(
    web::WKWebViewConfigurationProvider* config_provider,
    WKWebViewConfiguration* new_config) {
  user_content_controller_ = new_config.userContentController;
  ApplyContentRuleLists();
}

void AdBlockerContentRuleListProviderImpl::DoInstallContentRuleLists(
    std::vector<WKContentRuleList*> content_rule_lists) {
  base::EraseIf(content_rule_lists,
                [](const WKContentRuleList* content_rule_list) {
                  return content_rule_list == nullptr;
                });
  RemoveInstalledLists();
  installed_content_rule_lists_.swap(content_rule_lists);
  ApplyContentRuleLists();
  on_rules_applied_.Run();
}

void AdBlockerContentRuleListProviderImpl::RemoveInstalledLists() {
  for (auto* content_rule_list : installed_content_rule_lists_) {
    [user_content_controller_ removeContentRuleList:content_rule_list];
    [WKContentRuleListStore.defaultStore
        removeContentRuleListForIdentifier:content_rule_list.identifier
                         completionHandler:^(NSError* error) {
                           if (error) {
                             DLOG(WARNING) << "Failed removing rule list";
                           }
                         }];
  }
  installed_content_rule_lists_.clear();
}

void AdBlockerContentRuleListProviderImpl::ApplyContentRuleLists() {
  for (auto* content_rule_list : installed_content_rule_lists_) {
    [user_content_controller_ addContentRuleList:content_rule_list];
  }
}
}  // namespace

AdBlockerContentRuleListProvider::~AdBlockerContentRuleListProvider() = default;
/*static*/
std::unique_ptr<AdBlockerContentRuleListProvider>
AdBlockerContentRuleListProvider::Create(
    web::BrowserState* browser_state,
    RuleGroup group,
    base::RepeatingClosure on_rules_applied) {
  return std::make_unique<AdBlockerContentRuleListProviderImpl>(
      browser_state, group, on_rules_applied);
}

}  // namespace adblock_filter

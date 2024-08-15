// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ad_blocker/adblock_content_rule_list_provider.h"

#import <set>
#import <vector>

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/barrier_callback.h"
#import "base/barrier_closure.h"
#import "base/json/json_string_value_serializer.h"
#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "ios/ad_blocker/utils.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider_observer.h"

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
      base::OnceClosure on_loaded,
      base::RepeatingClosure on_done_applying_rules);
  ~AdBlockerContentRuleListProviderImpl() override;
  AdBlockerContentRuleListProviderImpl(
      const AdBlockerContentRuleListProviderImpl&) = delete;
  AdBlockerContentRuleListProviderImpl& operator=(
      const AdBlockerContentRuleListProviderImpl&) = delete;

  // Implementing AdBlockerContentRuleListProvider
  void SetIncognitoBrowserState(web::BrowserState* browser_state) override;
  void InstallContentRuleLists(const base::Value::List& lists) override;
  void ApplyLoadedRules() override;
  bool IsApplyingRules() override {
    return !list_application_in_progress_stamps_.empty();
  }

 private:
  // Implementing WKWebViewConfigurationProviderObserver
  void DidCreateNewConfiguration(
      web::WKWebViewConfigurationProvider* config_provider,
      WKWebViewConfiguration* new_config) override;

  void RemoveInstalledLists();
  void DoInstallContentRuleLists(
      int64_t rule_list_timestamp,
      std::vector<WKContentRuleList*> content_rule_lists);
  void ApplyContentRuleLists();
  void ApplyContentRuleListsForController(
      __weak WKUserContentController* user_content_controller);

  base::WeakPtr<web::WKWebViewConfigurationProvider> config_provider_;
  base::WeakPtr<web::WKWebViewConfigurationProvider> incognito_config_provider_;
  bool can_apply_rules_ = false;
  RuleGroup group_;
  __weak WKUserContentController* user_content_controller_;
  __weak WKUserContentController* incognito_user_content_controller_;
  std::vector<WKContentRuleList*> installed_content_rule_lists_;
  std::set<int64_t> list_application_in_progress_stamps_;
  base::RepeatingClosure on_done_applying_rules_;

  base::WeakPtrFactory<AdBlockerContentRuleListProviderImpl> weak_ptr_factory_{
      this};
};

AdBlockerContentRuleListProviderImpl::AdBlockerContentRuleListProviderImpl(
    web::BrowserState* browser_state,
    RuleGroup group,
    base::OnceClosure on_loaded,
    base::RepeatingClosure on_done_applying_rules)
    : config_provider_(
          web::WKWebViewConfigurationProvider::FromBrowserState(browser_state)
              .AsWeakPtr()),
      group_(group),
      on_done_applying_rules_(std::move(on_done_applying_rules)) {
  DidCreateNewConfiguration(config_provider_.get(),
                            config_provider_->GetWebViewConfiguration());
  config_provider_->AddObserver(this);

  base::WeakPtr<AdBlockerContentRuleListProviderImpl> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  __block base::OnceClosure on_loaded_helper(std::move(on_loaded));
  [WKContentRuleListStore.defaultStore getAvailableContentRuleListIdentifiers:^(
                                           NSArray<NSString*>* identifiers) {
    std::string list_prefix(kListNamePrefix);
    list_prefix.append(
        std::string(kListNameGroupPrefix[static_cast<size_t>(group_)]));
    std::vector<NSString*> relevant_identifiers;
    for (NSString* identifier in identifiers) {
      if ([identifier
              hasPrefix:[NSString stringWithUTF8String:list_prefix.c_str()]]) {
        relevant_identifiers.push_back(identifier);
      }
    }

    __block auto notify_on_loaded = base::BarrierClosure(
        relevant_identifiers.size(), std::move(on_loaded_helper));
    for (NSString* identifier : relevant_identifiers) {
      [WKContentRuleListStore.defaultStore
          lookUpContentRuleListForIdentifier:identifier
                           completionHandler:^(WKContentRuleList* rule_list,
                                               NSError* error) {
                             if (weak_this && rule_list) {
                               weak_this->installed_content_rule_lists_
                                   .push_back(rule_list);
                             }
                             notify_on_loaded.Run();
                           }];
    }
  }];
}

AdBlockerContentRuleListProviderImpl::~AdBlockerContentRuleListProviderImpl() {
  if (config_provider_) {
    config_provider_->RemoveObserver(this);
  }
  if (incognito_config_provider_) {
    incognito_config_provider_->RemoveObserver(this);
  }
}

void AdBlockerContentRuleListProviderImpl::ApplyLoadedRules() {
  can_apply_rules_ = true;
  ApplyContentRuleLists();
}

void AdBlockerContentRuleListProviderImpl::SetIncognitoBrowserState(
    web::BrowserState* browser_state) {
  if (incognito_config_provider_) {
    incognito_config_provider_->RemoveObserver(this);
    incognito_config_provider_.reset();
  }

  if (!browser_state) {
    return;
  }
  incognito_config_provider_ =
      web::WKWebViewConfigurationProvider::FromBrowserState(browser_state)
          .AsWeakPtr();
  DidCreateNewConfiguration(
      incognito_config_provider_.get(),
      incognito_config_provider_->GetWebViewConfiguration());
  incognito_config_provider_->AddObserver(this);
}

void AdBlockerContentRuleListProviderImpl::InstallContentRuleLists(
    const base::Value::List& lists) {
  if (lists.empty()) {
    RemoveInstalledLists();
    list_application_in_progress_stamps_.clear();
    on_done_applying_rules_.Run();
    return;
  }

  int64_t timestamp =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  list_application_in_progress_stamps_.insert(timestamp);
  std::string string_timestamp = base::NumberToString(timestamp);

  __block auto compile_and_apply = base::BarrierCallback<WKContentRuleList*>(
      lists.size(),
      base::BindOnce(
          &AdBlockerContentRuleListProviderImpl::DoInstallContentRuleLists,
          weak_ptr_factory_.GetWeakPtr(), timestamp));

  int list_index = 0;
  for (const auto& list : lists) {
    DCHECK(list.is_string());
    std::string list_name(kListNamePrefix);
    list_name.append(
        std::string(kListNameGroupPrefix[static_cast<size_t>(group_)]));

    list_name.append(string_timestamp);
    list_name.append("_");
    list_name.append(base::NumberToString(list_index++));

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
  if (config_provider == config_provider_.get()) {
    user_content_controller_ = new_config.userContentController;
    ApplyContentRuleListsForController(user_content_controller_);
  } else if (config_provider == incognito_config_provider_.get()) {
    incognito_user_content_controller_ = new_config.userContentController;
    ApplyContentRuleListsForController(incognito_user_content_controller_);
  }
}

void AdBlockerContentRuleListProviderImpl::DoInstallContentRuleLists(
    int64_t rule_list_timestamp,
    std::vector<WKContentRuleList*> content_rule_lists) {
  std::erase_if(content_rule_lists,
                [](const WKContentRuleList* content_rule_list) {
                  return content_rule_list == nullptr;
                });
  if (list_application_in_progress_stamps_.count(rule_list_timestamp) == 0) {
    // This list was removed from the list of lists to apply, probably because
    // of a request to remove all lists. Just get rid of the compilation result
    for (auto* content_rule_list : content_rule_lists) {
      [WKContentRuleListStore.defaultStore
          removeContentRuleListForIdentifier:content_rule_list.identifier
                           completionHandler:^(NSError* error) {
                             if (error) {
                               DLOG(WARNING) << "Failed removing rule list";
                             }
                           }];
    }
    return;
  }

  RemoveInstalledLists();
  installed_content_rule_lists_.swap(content_rule_lists);
  can_apply_rules_ = true;
  ApplyContentRuleLists();
  list_application_in_progress_stamps_.erase(rule_list_timestamp);
  if (list_application_in_progress_stamps_.empty())
    on_done_applying_rules_.Run();
}

void AdBlockerContentRuleListProviderImpl::RemoveInstalledLists() {
  for (auto* content_rule_list : installed_content_rule_lists_) {
    if (user_content_controller_) {
      [user_content_controller_ removeContentRuleList:content_rule_list];
    }
    if (incognito_user_content_controller_) {
      [incognito_user_content_controller_
          removeContentRuleList:content_rule_list];
    }
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
  if (user_content_controller_)
    ApplyContentRuleListsForController(user_content_controller_);
  if (incognito_user_content_controller_)
    ApplyContentRuleListsForController(incognito_user_content_controller_);
}

void AdBlockerContentRuleListProviderImpl::ApplyContentRuleListsForController(
    __weak WKUserContentController* user_content_controller) {
  if (!can_apply_rules_) {
    return;
  }

  for (auto* content_rule_list : installed_content_rule_lists_) {
    [user_content_controller addContentRuleList:content_rule_list];
  }
}
}  // namespace

AdBlockerContentRuleListProvider::~AdBlockerContentRuleListProvider() = default;
/*static*/
std::unique_ptr<AdBlockerContentRuleListProvider>
AdBlockerContentRuleListProvider::Create(
    web::BrowserState* browser_state,
    RuleGroup group,
    base::OnceClosure on_loaded,
    base::RepeatingClosure on_done_applying_rules) {
  return std::make_unique<AdBlockerContentRuleListProviderImpl>(
      browser_state, group, std::move(on_loaded), on_done_applying_rules);
}

}  // namespace adblock_filter

// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ad_blocker/adblock_rule_service_impl.h"

#import <algorithm>
#import <memory>
#import <utility>
#import <vector>

#import <WebKit/WebKit.h>

#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/ad_blocker/adblock_known_sources_handler.h"
#import "components/ad_blocker/adblock_rule_manager_impl.h"
#import "components/ad_blocker/adblock_rule_source_handler.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace adblock_filter {
RuleServiceImpl::RuleServiceImpl(
    web::BrowserState* browser_state,
    RuleSourceHandler::RulesCompiler rules_compiler,
    std::string locale)
    : browser_state_(browser_state),
      rules_compiler_(std::move(rules_compiler)),
      locale_(std::move(locale)) {}
RuleServiceImpl::~RuleServiceImpl() {
  if (is_loaded_)
    web::WKWebViewConfigurationProvider::FromBrowserState(browser_state_)
        .RemoveObserver(this);
}

void RuleServiceImpl::AddObserver(RuleService::Observer* observer) {
  observers_.AddObserver(observer);
}

void RuleServiceImpl::RemoveObserver(RuleService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RuleServiceImpl::Load() {
  DCHECK(!is_loaded_ && !state_store_);
  file_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  state_store_.emplace(browser_state_->GetStatePath(), this, file_task_runner_);

  // Unretained is safe because we own the sources store
  state_store_->Load(
      base::BindOnce(&RuleServiceImpl::OnStateLoaded, base::Unretained(this)));
}

bool RuleServiceImpl::IsLoaded() const {
  return is_loaded_;
}

void RuleServiceImpl::Shutdown() {
  if (is_loaded_) {
    state_store_->OnRuleServiceShutdown();
    rule_manager_->RemoveObserver(this);
  }
}

bool RuleServiceImpl::IsRuleGroupEnabled(RuleGroup group) const {
  return true;
}

void RuleServiceImpl::SetRuleGroupEnabled(RuleGroup group, bool enabled) {
  DCHECK(is_loaded_);
  if (IsRuleGroupEnabled(group) == enabled)
    return;

  NOTREACHED();
  // TODO(juliem): Implement

  for (RuleService::Observer& observer : observers_)
    observer.OnGroupStateChanged(group);

  state_store_->ScheduleSave();
}

void RuleServiceImpl::OnStateLoaded(
    RuleServiceStorage::LoadResult load_result) {
  // All cases of base::Unretained here are safe. We are generally passing
  // callbacks to objects that we own, calling to either this or other objects
  // that we own.

  rule_manager_.emplace(
      file_task_runner_, browser_state_->GetStatePath(),
      browser_state_->GetSharedURLLoaderFactory(),
      std::move(load_result.rule_sources),
      std::move(load_result.active_exceptions_lists),
      std::move(load_result.exceptions),
      base::BindRepeating(&RuleServiceStorage::ScheduleSave,
                          base::Unretained(&state_store_.value())),
      rules_compiler_, base::DoNothing());
  rule_manager_->AddObserver(this);

  known_sources_handler_.emplace(
      this, load_result.storage_version, locale_, load_result.known_sources,
      std::move(load_result.deleted_presets),
      base::BindRepeating(&RuleServiceStorage::ScheduleSave,
                          base::Unretained(&state_store_.value())));

  content_rule_list_provider_.emplace();
  web::WKWebViewConfigurationProvider& config_provider =
      web::WKWebViewConfigurationProvider::FromBrowserState(browser_state_);
  DidCreateNewConfiguration(&config_provider,
                            config_provider.GetWebViewConfiguration());
  config_provider.AddObserver(this);

  is_loaded_ = true;
  for (RuleService::Observer& observer : observers_)
    observer.OnRuleServiceStateLoaded(this);
}

void RuleServiceImpl::DidCreateNewConfiguration(
    web::WKWebViewConfigurationProvider* config_provider,
    WKWebViewConfiguration* new_config) {
  content_rule_list_provider_->SetUserContentController(
      new_config.userContentController);
}

// We have no rules index on iOS
std::string RuleServiceImpl::GetRulesIndexChecksum(RuleGroup group) {
  return "";
}

RuleManager* RuleServiceImpl::GetRuleManager() {
  DCHECK(rule_manager_);
  return &rule_manager_.value();
}

KnownRuleSourcesHandler* RuleServiceImpl::GetKnownSourcesHandler() {
  DCHECK(known_sources_handler_);
  return &known_sources_handler_.value();
}

BlockedUrlsReporter* RuleServiceImpl::GetBlockerUrlsReporter() {
  return nullptr;
}

void RuleServiceImpl::OnExceptionListChanged(RuleGroup group,
                                             RuleManager::ExceptionsList list) {
}
}  // namespace adblock_filter

// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/ios/adblock_rule_service_impl.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/containers/enum_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/ad_blocker/core/adblock_rule_manager_impl.h"
#include "components/ad_blocker/core/adblock_rule_source_handler.h"
#include "components/ad_blocker/ios/adblock_content_injection_handler.h"
#include "components/ad_blocker/ios/adblock_content_rule_list_provider.h"
#include "components/ad_blocker/public/core/adblock_known_sources_handler.h"
#include "ios/web/public/browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace adblock_filter {

namespace {
void DeleteLeakedCompilationResult() {
  // Webkit fails to delete intermediary compilation results if the browser is
  // shut down while compilation takes place. We clean up for it here.
  base::FilePath temp_dir;
  if (!base::PathService::Get(base::DIR_TEMP, &temp_dir))
    return;
  base::FileEnumerator enumerator(temp_dir, false, base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("ContentRuleList*"));

  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    base::DeleteFile(path);
  }
}

class Loader {
 public:
  using LoadedCallback = base::OnceCallback<void(
      RuleGroupArray<std::unique_ptr<AdBlockerContentRuleListProvider>>
          loaded_content_rule_list_providers,
      RuleServiceStorageDelegate::LoadResult load_result)>;
  using DoneApplyingRulesCallback = base::RepeatingCallback<void(RuleGroup)>;

  Loader(web::BrowserState* browser_state,
         LoadedCallback on_loaded,
         DoneApplyingRulesCallback on_done_applying_rules)
      : on_loaded_(std::move(on_loaded)) {
    for (auto [group, loading_content_rule_list_providers] :
         loading_content_rule_list_providers_) {
      loading_content_rule_list_providers =
          AdBlockerContentRuleListProvider::Create(
              browser_state, group,
              base::BindOnce(&Loader::OnContentRuleListProviderLoaded,
                             base::Unretained(this), group),
              base::BindRepeating(on_done_applying_rules, group));
    }

    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&DeleteLeakedCompilationResult),
        base::BindOnce(&Loader::OnLeakedCompilationResultDeleted,
                       base::Unretained(this)));
  }

  Loader(const Loader&) = delete;
  Loader& operator=(const Loader&) = delete;

  void OnContentRuleListProviderLoaded(RuleGroup rule_group) {
    loaded_groups_.Put(rule_group);
    CallOnLoadedIfReady();
  }

  void OnLoadedFromStorage(RuleServiceStorageDelegate::LoadResult load_result) {
    load_result_ = std::move(load_result);
    loaded_flags_.Put(kLoadedFromStorage);
    CallOnLoadedIfReady();
  }

  void OnLeakedCompilationResultDeleted() {
    loaded_flags_.Put(kLeakedCompilationResultDeleted);
    CallOnLoadedIfReady();
  }

  void CallOnLoadedIfReady() {
    if (!(loaded_flags_.HasAll(loaded_flags_.All()) &&
          loaded_groups_.HasAll(loaded_groups_.All()))) {
      return;
    }

    std::move(on_loaded_)
        .Run(std::move(loading_content_rule_list_providers_),
             std::move(load_result_));
    delete this;
  }

 private:
  enum LoadedFlags {
    kLoadedFromStorage = 0,
    kLeakedCompilationResultDeleted,

    kLoadedFlagsMin = kLoadedFromStorage,
    kLoadedFlagsMax = kLeakedCompilationResultDeleted,
  };

  ~Loader() = default;
  base::EnumSet<LoadedFlags, kLoadedFlagsMin, kLoadedFlagsMax> loaded_flags_;
  base::EnumSet<RuleGroup, RuleGroup::kFirst, RuleGroup::kLast> loaded_groups_;
  LoadedCallback on_loaded_;
  RuleGroupArray<std::unique_ptr<AdBlockerContentRuleListProvider>>
      loading_content_rule_list_providers_;
  RuleServiceStorageDelegate::LoadResult load_result_;
};

}  // namespace

RuleServiceImpl::RuleServiceImpl(
    web::BrowserState* browser_state,
    PrefService* prefs,
    RuleSourceHandler::RulesCompiler rules_compiler,
    std::string locale)
    : browser_state_(browser_state),
      prefs_(prefs),
      rules_compiler_(std::move(rules_compiler)),
      locale_(std::move(locale)) {
  pref_change_registrar_.Init(prefs_);
}

RuleServiceImpl::~RuleServiceImpl() = default;

void RuleServiceImpl::AddObserver(RuleService::Observer* observer) {
  observers_.AddObserver(observer);
}

void RuleServiceImpl::RemoveObserver(RuleService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RuleServiceImpl::Load() {
  CHECK(!is_loaded_ && !state_store_);
  file_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  resources_.emplace(file_task_runner_.get());

  content_injection_handler_ =
      ContentInjectionHandler::Create(browser_state_, &resources_.value());

  Loader* loader =
      new Loader(browser_state_,
                 base::BindOnce(&RuleServiceImpl::OnFullyLoaded,
                                weak_ptr_factory_.GetWeakPtr()),
                 base::BindRepeating(&RuleServiceImpl::OnDoneApplyingRules,
                                     weak_ptr_factory_.GetWeakPtr()));
  on_storage_loaded_ =
      base::BindOnce(&Loader::OnLoadedFromStorage, base::Unretained(loader));

  state_store_.emplace(browser_state_->GetStatePath(), this, file_task_runner_);
  state_store_->Load();
}

void RuleServiceImpl::OnStorageDoneLoading(
    RuleServiceStorageDelegate::LoadResult load_result) {
  std::move(on_storage_loaded_).Run(std::move(load_result));
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

void RuleServiceImpl::SetIncognitoBrowserState(
    web::BrowserState* browser_state) {
  incognito_browser_state_ = browser_state;
  if (is_loaded_) {
    for (auto [group, organized_rules_manager] : organized_rules_manager_) {
      organized_rules_manager->SetIncognitoBrowserState(browser_state);
    }
  }

  content_injection_handler_->SetIncognitoBrowserState(browser_state);
}

bool RuleServiceImpl::IsPartnerListAllowedDocument(RuleGroup group, GURL url) {
  const base::Value::List& partner_list_allowed_documents =
      organized_rules_manager_[group]->partner_list_allowed_documents();

  for (const auto& partner_list_allowed_document :
       partner_list_allowed_documents) {
    const std::string& host = partner_list_allowed_document.GetString();
    if (!url.host().ends_with(host)) {
      continue;
    }

    if (url.host().size() == host.size()) {
      return true;
    }

    if (url.host().size() > host.size()) {
      size_t size_diff = url.host().size() - host.size();

      if (url.host().at(size_diff - 1) == '.') {
        return true;
      }
    }
  }

  return false;
}

void RuleServiceImpl::OnFullyLoaded(
    RuleGroupArray<std::unique_ptr<AdBlockerContentRuleListProvider>>
        loaded_content_rule_list_providers,
    RuleServiceStorageDelegate::LoadResult load_result) {
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

  // Unretained is ok, since we own the registrar and it owns the callback.
  pref_change_registrar_.Add(
      vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking,
      base::BindRepeating(&RuleServiceImpl::OnEnableDocumentBlockingChanged,
                          base::Unretained(this)));

  known_sources_handler_.emplace(
      &rule_manager_.value(), load_result.storage_version, locale_,
      load_result.known_sources, std::move(load_result.deleted_presets),
      base::BindRepeating(&RuleServiceStorage::ScheduleSave,
                          base::Unretained(&state_store_.value())));

  for (auto [group, organized_rules_manager] : organized_rules_manager_) {
    organized_rules_manager.emplace(
        &rule_manager_.value(),
        std::move(loaded_content_rule_list_providers[group]),
        content_injection_handler_.get(), group, browser_state_->GetStatePath(),
        load_result.index_checksums[group],
        base::BindRepeating(&RuleServiceImpl::OnRulesIndexChanged,
                            base::Unretained(this), group),
        base::BindRepeating(&RuleManager::OnCompiledRulesReadFailCallback,
                            base::Unretained(&rule_manager_.value())),
        base::BindRepeating(&RuleServiceImpl::OnStartApplyingRules,
                            base::Unretained(this), group),
        file_task_runner_);

    organized_rules_manager->SetIncognitoBrowserState(incognito_browser_state_);
  }

  is_loaded_ = true;
  for (RuleService::Observer& observer : observers_)
    observer.OnRuleServiceStateLoaded(this);
}

bool RuleServiceImpl::IsApplyingRules(RuleGroup group) {
  return organized_rules_manager_[group]->IsApplyingRules();
}

std::string RuleServiceImpl::GetRulesIndexChecksum(RuleGroup group) {
  CHECK(is_loaded_);
  CHECK(organized_rules_manager_[group]);
  return organized_rules_manager_[group]->organized_rules_checksum();
}

RuleService::IndexBuildResult RuleServiceImpl::GetRulesIndexBuildResult(
    RuleGroup group) {
  CHECK(is_loaded_);
  CHECK(organized_rules_manager_[group]);
  return organized_rules_manager_[group]->build_result();
}

RuleManager* RuleServiceImpl::GetRuleManager() {
  CHECK(is_loaded_);
  CHECK(rule_manager_);
  return &rule_manager_.value();
}

KnownRuleSourcesHandler* RuleServiceImpl::GetKnownSourcesHandler() {
  CHECK(is_loaded_);
  CHECK(known_sources_handler_);
  return &known_sources_handler_.value();
}

void RuleServiceImpl::OnExceptionListChanged(RuleGroup group,
                                             RuleManager::ExceptionsList list) {
}

void RuleServiceImpl::OnRulesIndexChanged(
    RuleGroup group,
    RuleService::IndexBuildResult build_result) {
  // The state store will read all checksums when saving. No need to worry about
  // which has changed.
  state_store_->ScheduleSave();
  for (RuleService::Observer& observer : observers_)
    observer.OnRulesIndexBuilt(group, build_result);
}

void RuleServiceImpl::OnEnableDocumentBlockingChanged() {
  // Force a recompilation of all sources
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    for (const auto& [source_id, _] : rule_manager_->GetRuleSources(group)) {
      rule_manager_->FetchRuleSourceNow(group, source_id);
    }
  }
}

void RuleServiceImpl::OnStartApplyingRules(RuleGroup group) {
  for (RuleService::Observer& observer : observers_) {
    observer.OnStartApplyingRules(group);
  }
}

void RuleServiceImpl::OnDoneApplyingRules(RuleGroup group) {
  // We receive this signal when the AdBlockerContentRuleListProvider is done
  // with all processing, but the OrganizedRulesManager may have started with
  // new processing that has not yet reached the
  // AdBlockerContentRuleListProvider. We block the signal when that happens.
  if (IsApplyingRules(group))
    return;

  state_store_->ScheduleSave();

  for (RuleService::Observer& observer : observers_) {
    observer.OnDoneApplyingRules(group);
  }
}

}  // namespace adblock_filter

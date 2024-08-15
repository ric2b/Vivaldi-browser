// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "ios/ad_blocker/adblock_organized_rules_manager.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/ad_blocker/utils.h"
#include "ios/ad_blocker/adblock_content_injection_handler.h"
#include "ios/ad_blocker/adblock_content_rule_list_provider.h"
#include "ios/ad_blocker/adblock_rules_organizer.h"
#include "ios/ad_blocker/ios_rules_compiler.h"
#include "ios/ad_blocker/utils.h"

namespace adblock_filter {

namespace {
const base::FilePath::StringType kOrganizedRulesFileName =
    FILE_PATH_LITERAL("Organized");

std::unique_ptr<base::Value> GetJsonFromFile(
    const base::FilePath& compiled_rules_path,
    const std::string& checksum,
    const int expected_version) {
  std::string file_contents;
  base::ReadFileToString(compiled_rules_path, &file_contents);

  if (checksum != CalculateBufferChecksum(file_contents))
    return nullptr;

  std::unique_ptr<base::Value> result =
      JSONStringValueDeserializer(file_contents).Deserialize(nullptr, nullptr);

  DCHECK(result->is_dict());
  auto& result_dict = result->GetDict();
  std::optional<int> version = result_dict.FindInt(rules_json::kVersion);
  if (version.value_or(0) != expected_version) {
    return nullptr;
  }
  return result;
}

std::pair<std::string, base::Value> WriteRulesAndGetChecksum(
    const base::FilePath& filename,
    base::Value non_ios_rules_and_metadata) {
  std::string json;
  if (!JSONStringValueSerializer(&json).Serialize(non_ios_rules_and_metadata))
    NOTREACHED();
  if (!base::WriteFile(filename, json))
    return {"", std::move(non_ios_rules_and_metadata)};
  return {CalculateBufferChecksum(json), std::move(non_ios_rules_and_metadata)};
}
}  // namespace

OrganizedRulesManager::OrganizedRulesManager(
    RuleService* rule_service,
    std::unique_ptr<AdBlockerContentRuleListProvider>
        content_rule_list_provider,
    ContentInjectionHandler* content_injection_handler,
    RuleGroup group,
    base::FilePath browser_state_path,
    const std::string& organized_rules_checksum,
    OrganizedRulesChangedCallback organized_rules_changed_callback,
    RulesReadFailCallback rule_read_fail_callback,
    base::RepeatingClosure on_start_applying_rules,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : rule_manager_(rule_service->GetRuleManager()),
      content_rule_list_provider_(std::move(content_rule_list_provider)),
      content_injection_handler_(content_injection_handler),
      group_(group),
      is_loaded_(false),
      rules_list_folder_(browser_state_path.Append(GetRulesFolderName())
                             .Append(GetGroupFolderName(group_))),
      organized_rules_changed_callback_(organized_rules_changed_callback),
      rule_read_fail_callback_(rule_read_fail_callback),
      on_start_applying_rules_(on_start_applying_rules),
      file_task_runner_(file_task_runner) {
  organized_rules_checksum_ = organized_rules_checksum;
  if (organized_rules_checksum_.empty()) {
    // We want OnOrganizedRulesLoaded to execute after all loading has been
    // done, to avoid slowing things down.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&OrganizedRulesManager::OnOrganizedRulesLoaded,
                       weak_factory_.GetWeakPtr(), nullptr));
  } else {
    file_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&GetJsonFromFile,
                       rules_list_folder_.Append(kOrganizedRulesFileName),
                       organized_rules_checksum_,
                       GetOrganizedRulesVersionNumber()),
        base::BindOnce(&OrganizedRulesManager::OnOrganizedRulesLoaded,
                       weak_factory_.GetWeakPtr()));
  }
}

OrganizedRulesManager::~OrganizedRulesManager() {
  rule_manager_->RemoveObserver(this);
}

void OrganizedRulesManager::SetIncognitoBrowserState(
    web::BrowserState* browser_state) {
  content_rule_list_provider_->SetIncognitoBrowserState(browser_state);
}

void OrganizedRulesManager::OnRuleSourceUpdated(
    RuleGroup group,
    const ActiveRuleSource& rule_source) {
  if (group != group_ || rule_source.is_fetching)
    return;

  // If the last fetch failed, either we won't have anything to read, or
  // the rules won't have changed, so skip reading. `kFileUnsupported` results
  // from a successful fetch with no valid rules.
  if (rule_source.last_fetch_result == FetchResult::kSuccess ||
      rule_source.last_fetch_result == FetchResult::kFileUnsupported) {
    const auto& old_source = rule_sources_.find(rule_source.core.id());

    if (old_source == rule_sources_.end() ||
        rule_source.rules_list_checksum !=
            old_source->second.rules_list_checksum)
      ReadCompiledRules(rule_source);
    // Make sure to drop the |old_source| iterator here sisnce we're about to
    // change the map.
  }

  rule_sources_.insert_or_assign(rule_source.core.id(), rule_source);
}

void OrganizedRulesManager::OnRuleSourceDeleted(uint32_t source_id,
                                                RuleGroup group) {
  if (group != group_) {
    return;
  }

  rule_sources_.erase(source_id);
  compiled_rules_.erase(source_id);

  ReorganizeRules();
}

void OrganizedRulesManager::OnExceptionListStateChanged(RuleGroup group) {
  if (group != group_)
    return;
  UpdateExceptions();
}

void OrganizedRulesManager::OnExceptionListChanged(
    RuleGroup group,
    RuleManager::ExceptionsList list) {
  if (group != group_)
    return;
  if (rule_manager_->GetActiveExceptionList(group_) == list)
    UpdateExceptions();
}

void OrganizedRulesManager::ReadCompiledRules(
    const ActiveRuleSource& rule_source) {
  if (rule_source.last_fetch_result == FetchResult::kFileUnsupported) {
    // We know there is no valid rules here. No point in trying.
    // Keep any rules buffer around for the index currently in use, they'll be
    // cleared once the new index is ready.
    if (compiled_rules_.count(rule_source.core.id()) != 0) {
      compiled_rules_.erase(rule_source.core.id());
      ReorganizeRules();
    }
    return;
  }

  CHECK(!rule_source.rules_list_checksum.empty());

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetJsonFromFile,
                     rules_list_folder_.AppendASCII(
                         base::NumberToString(rule_source.core.id())),
                     rule_source.rules_list_checksum,
                     GetIntermediateRepresentationVersionNumber()),
      base::BindOnce(&OrganizedRulesManager::OnRulesRead,
                     weak_factory_.GetWeakPtr(), rule_source.core.id(),
                     rule_source.rules_list_checksum));
}

void OrganizedRulesManager::OnRulesRead(
    uint32_t source_id,
    const std::string& checksum,
    std::unique_ptr<base::Value> compiled_rules) {
  const auto& rule_source = rule_sources_.find(source_id);

  if (rule_source == rule_sources_.end()) {
    // The rule source was removed while we were fetching its buffer.
    return;
  }

  if (rule_source->second.rules_list_checksum != checksum) {
    // The rule source was modified while we were fetching its buffer.
    return;
  }

  if (!compiled_rules) {
    // If we had compiled rules for this source already, keep them for now.
    rule_read_fail_callback_.Run(group_, source_id);
    return;
  }

  compiled_rules_[source_id] = base::MakeRefCounted<CompiledRules>(
      std::move(*compiled_rules), std::move(checksum));

  ReorganizeRules();
}

void OrganizedRulesManager::UpdateExceptions() {
  RuleManager::ExceptionsList active_list =
      rule_manager_->GetActiveExceptionList(group_);
  const std::set<std::string>& exceptions =
      rule_manager_->GetExceptions(group_, active_list);

  if (exceptions.empty()) {
    exception_rule_ = base::Value();
  } else {
    exception_rule_ = CompileExceptionsRule(
        exceptions, active_list == RuleManager::ExceptionsList::kProcessList);
  }

  ReorganizeRules();
}

void OrganizedRulesManager::ReorganizeRules() {
  if (!is_loaded_) {
    // Wait until everything has been loaded before we try to run this, if it is
    // needed.
    return;
  }

  organized_rules_ready_callback_.Reset(
      base::BindOnce(&OrganizedRulesManager::OnOrganizedRulesReady,
                     weak_factory_.GetWeakPtr()));

  if (rule_sources_.empty())
    Disable();

  if (rule_manager_->GetActiveExceptionList(group_) ==
          RuleManager::ExceptionsList::kProcessList &&
      exception_rule_.is_none()) {
    Disable();
    return;
  }

  on_start_applying_rules_.Run();

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&OrganizeRules, compiled_rules_, exception_rule_.Clone()),
      organized_rules_ready_callback_.callback());
}

void OrganizedRulesManager::OnOrganizedRulesLoaded(
    std::unique_ptr<base::Value> non_ios_rules_and_metadata) {
  rule_sources_ = rule_manager_->GetRuleSources(group_),
  rule_manager_->AddObserver(this);

  UpdateExceptions();

  for (const auto& [rule_source_id, rule_source] : rule_sources_) {
    if (!rule_source.rules_list_checksum.empty()) {
      ReadCompiledRules(rule_source);
    }
  }

  if (rule_sources_.empty()) {
    Disable();
    return;
  }

  bool organized_rules_ok =
      CheckOrganizedRules(non_ios_rules_and_metadata.get());

  if (organized_rules_ok) {
    base::Value::Dict* scriptlet_rules =
        non_ios_rules_and_metadata->GetDict().FindDict(
            rules_json::kScriptletRules);
    if (scriptlet_rules)
      content_injection_handler_->SetScriptletInjectionRules(
          group_, std::move(*scriptlet_rules));

    content_rule_list_provider_->ApplyLoadedRules();
  } else {
    organized_rules_checksum_.clear();
  }

  // Queue firing OnAllRulesLoaded after all the loads queued in
  // ReadCompiledRules. This ensures that OnAllRulesLoaded is queued after
  // all of the OnRulesRead callbacks.
  file_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&OrganizedRulesManager::OnAllRulesLoaded,
                     weak_factory_.GetWeakPtr(), !organized_rules_ok));
}

bool OrganizedRulesManager::CheckOrganizedRules(
    base::Value* non_ios_rules_and_metadata) {
  if (!non_ios_rules_and_metadata) {
    return false;
  }
  DCHECK(non_ios_rules_and_metadata->is_dict());

  // Older versions of the files contained all the rules and were systematically
  // used to reload rules on startup. If we get one of those old versions, we
  // can't assume that the rules stored on the webkit side are sound. Try
  // starting fresh instead.
  if (non_ios_rules_and_metadata->GetDict().contains("organized-rules")) {
    content_rule_list_provider_->InstallContentRuleLists(base::Value::List());
    return false;
  }

  base::Value::Dict* metadata =
      non_ios_rules_and_metadata->GetDict().FindDict(rules_json::kMetadata);
  if (!metadata) {
    return false;
  }

  base::Value::Dict* list_checksums =
      metadata->FindDict(rules_json::kListChecksums);
  DCHECK(list_checksums);

  size_t valid_count = 0;
  for (const auto& [rule_source_id, rule_source] : rule_sources_) {
    if (!rule_source.rules_list_checksum.empty()) {
      std::string string_id = base::NumberToString(rule_source.core.id());
      ++valid_count;
      auto* compiled_checksum = list_checksums->Find(string_id);
      if (!compiled_checksum ||
          compiled_checksum->GetString() != rule_source.rules_list_checksum) {
        return false;
      }
    }
  }

  if (list_checksums->size() != valid_count) {
    return false;
  }

  std::string* exceptions_checksum =
      metadata->FindString(rules_json::kExceptionRule);
  if ((exceptions_checksum == nullptr) != exception_rule_.is_none()) {
    return false;
  }

  if (exceptions_checksum) {
    DCHECK(exception_rule_.is_dict());
    std::string serialized_exception;
    if (!JSONStringValueSerializer(&serialized_exception)
             .Serialize(exception_rule_))
      NOTREACHED();
    if (*exceptions_checksum != CalculateBufferChecksum(serialized_exception)) {
      return false;
    }
  }

  return true;
}

void OrganizedRulesManager::OnAllRulesLoaded(bool should_reorganize_rules) {
  is_loaded_ = true;
  if (should_reorganize_rules) {
    ReorganizeRules();
  }
}

void OrganizedRulesManager::Disable() {
  content_rule_list_provider_->InstallContentRuleLists(base::Value::List());
  build_result_ = RuleService::kBuildSuccess;
  organized_rules_checksum_.clear();
  organized_rules_changed_callback_.Run(build_result_);
}

void OrganizedRulesManager::OnOrganizedRulesReady(base::Value rules) {
  build_result_ = rules.is_none() ? RuleService::kTooManyAllowRules
                                  : RuleService::kBuildSuccess;
  if (build_result_ != RuleService::kBuildSuccess) {
    organized_rules_changed_callback_.Run(build_result_);
    return;
  }

  DCHECK(rules.is_dict());
  base::Value::List* ios_content_blocker_rules_rules =
      rules.GetDict().FindList(rules_json::kIosContentBlockerRules);
  DCHECK(ios_content_blocker_rules_rules);
  content_rule_list_provider_->InstallContentRuleLists(
      *ios_content_blocker_rules_rules);

  base::Value::Dict* non_ios_rules_and_metadata =
      rules.GetDict().FindDict(rules_json::kNonIosRulesAndMetadata);
  DCHECK(non_ios_rules_and_metadata);

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteRulesAndGetChecksum,
                     rules_list_folder_.Append(kOrganizedRulesFileName),
                     base::Value(std::move(*non_ios_rules_and_metadata))),
      base::BindOnce(
          [](base::WeakPtr<OrganizedRulesManager> self,
             RuleService::IndexBuildResult build_result,
             std::pair<std::string, base::Value> checksum_and_rules) {
            auto& [checksum, non_ios_rules] = checksum_and_rules;
            base::Value::Dict* scriptlet_rules =
                non_ios_rules.GetDict().FindDict(rules_json::kScriptletRules);
            if (scriptlet_rules) {
              self->content_injection_handler_->SetScriptletInjectionRules(
                  self->group_, std::move(*scriptlet_rules));
            }
            if (self && !checksum.empty()) {
              self->organized_rules_checksum_ = checksum;
              self->organized_rules_changed_callback_.Run(build_result);
            }
          },
          weak_factory_.GetWeakPtr(), build_result_));
}

bool OrganizedRulesManager::IsApplyingRules() const {
  return !organized_rules_ready_callback_.callback().is_null() ||
         content_rule_list_provider_->IsApplyingRules();
}
}  // namespace adblock_filter

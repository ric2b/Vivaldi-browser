// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "ios/ad_blocker/adblock_organized_rules_manager.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/ad_blocker/utils.h"
#include "ios/ad_blocker/adblock_content_rule_list_provider.h"
#include "ios/ad_blocker/adblock_rules_organizer.h"
#include "ios/ad_blocker/ios_rules_compiler.h"
#include "ios/ad_blocker/utils.h"

namespace adblock_filter {
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
  absl::optional<int> version = result_dict.FindInt(rules_json::kVersion);
  if (version.value_or(0) != expected_version) {
    return nullptr;
  }
  return result;
}

// WriteFile takes a StringPiece. We need to ensure that the backing for it
// doesn't go out of scope when called from a different sequence.
bool WriteFileWrapper(const base::FilePath& filename, std::string data) {
  return base::WriteFile(filename, data);
}

std::string WriteRulesAndGetChecksum(const base::FilePath& filename,
                                     base::Value rules) {
  std::string json;
  if (!JSONStringValueSerializer(&json).Serialize(rules))
    NOTREACHED();
  if (!base::WriteFile(filename, json))
    return "";
  return CalculateBufferChecksum(json);
}

OrganizedRulesManager::OrganizedRulesManager(
    RuleService* rule_service,
    std::unique_ptr<AdBlockerContentRuleListProvider>
        content_rule_list_provider,
    RuleGroup group,
    base::FilePath browser_state_path,
    const std::string& organized_rules_checksum,
    OrganizedRulesChangedCallback organized_rules_changed_callback,
    RulesReadFailCallback rule_read_fail_callback,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : rule_manager_(rule_service->GetRuleManager()),
      content_rule_list_provider_(std::move(content_rule_list_provider)),
      group_(group),
      is_loaded_(false),
      rule_sources_(rule_manager_->GetRuleSources(group)),
      rules_list_folder_(browser_state_path.Append(GetRulesFolderName())
                             .Append(GetGroupFolderName(group_))),
      organized_rules_changed_callback_(organized_rules_changed_callback),
      rule_read_fail_callback_(rule_read_fail_callback),
      file_task_runner_(file_task_runner) {
  rule_manager_->AddObserver(this);

/*  {
    std::string test_rules_str = R"===(
      {
        "network": {
          "block": {
            "specific": [
              {
               "action" : {
                  "type" : "block"
               },
               "trigger" : {
                  "if-top-url" : [
                     "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?testpages\\.adblockplus\\.org[:\\/]"
                  ],
                  "resource-type" : [
                     "raw"
                  ],
                  "top-url-filter-is-case-sensitive" : true,
                  "url-filter" : ".*",
                  "url-filter-is-case-sensitive" : false
               }
              }
            ]
          }
        }
      }
    )===";
    std::string checksum = CalculateBufferChecksum(test_rules_str);
    auto test_rules = JSONStringValueDeserializer(test_rules_str)
                          .Deserialize(nullptr, nullptr);
    DCHECK(test_rules);
    compiled_rules_[0] = base::MakeRefCounted<CompiledRules>(
        std::move(*test_rules), std::move(checksum));
  }*/

  UpdateExceptions();

  for (const auto& [rule_source_id, rule_source] : rule_sources_) {
    ReadCompiledRules(rule_source);
  }

  if (organized_rules_checksum.empty()) {
    // We don't have organized rules. Just make sure OnOrganizedRulesLoaded is
    // called after all the ruleshave been read to trigger a rebuild.
    file_task_runner->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindOnce(&OrganizedRulesManager::OnOrganizedRulesLoaded,
                       weak_factory_.GetWeakPtr(), "", nullptr));
  } else {
    file_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&GetJsonFromFile,
                       rules_list_folder_.Append(kOrganizedRulesFileName),
                       organized_rules_checksum,
                       GetOrganizedRulesVersionNumber()),
        base::BindOnce(&OrganizedRulesManager::OnOrganizedRulesLoaded,
                       weak_factory_.GetWeakPtr(), organized_rules_checksum));
  }
}

OrganizedRulesManager::~OrganizedRulesManager() {
  rule_manager_->RemoveObserver(this);
}

void OrganizedRulesManager::OnRulesSourceUpdated(
    const RuleSource& rule_source) {
  if (rule_source.group != group_ || rule_source.is_fetching)
    return;

  ReadCompiledRules(rule_source);
  rule_sources_.insert_or_assign(rule_source.id, rule_source);
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

void OrganizedRulesManager::ReadCompiledRules(const RuleSource& rule_source) {
  const auto existing_rules = compiled_rules_.find(rule_source.id);
  if (existing_rules != compiled_rules_.end() &&
      existing_rules->second->checksum() == rule_source.rules_list_checksum) {
    // checksum hasn't changed. Don't re-index.
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &GetJsonFromFile,
          rules_list_folder_.AppendASCII(base::NumberToString(rule_source.id)),
          rule_source.rules_list_checksum,
          GetIntermediateRepresentationVersionNumber()),
      base::BindOnce(&OrganizedRulesManager::OnRulesRead,
                     weak_factory_.GetWeakPtr(), rule_source.id,
                     rule_source.rules_list_checksum));
}

void OrganizedRulesManager::OnRulesRead(
    uint32_t source_id,
    const std::string& checksum,
    std::unique_ptr<base::Value> compiled_rules) {
  if (!rule_sources_.count(source_id)) {
    // The rule source was removed while we were fetching its compiled rules.
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

  if (rule_manager_->GetActiveExceptionList(group_) ==
          RuleManager::ExceptionsList::kProcessList &&
      exception_rule_.is_none()) {
    Disable();
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&OrganizeRules, compiled_rules_, exception_rule_.Clone()),
      organized_rules_ready_callback_.callback());
}

void OrganizedRulesManager::OnOrganizedRulesLoaded(
    std::string checksum,
    std::unique_ptr<base::Value> rules) {
  is_loaded_ = true;
  if (!rules) {
    if (!rule_sources_.empty())
      ReorganizeRules();
    return;
  }
  DCHECK(rules->is_dict());
  base::Value::Dict* lists_info =
      rules->GetDict().FindDict(rules_json::kListsInfo);
  DCHECK(lists_info);

  if (lists_info->size() != compiled_rules_.size()) {
    ReorganizeRules();
  } else {
    for (auto [string_id, list_checksum] : *lists_info) {
      DCHECK(list_checksum.is_string());
      uint32_t id = 0;
      bool result = base::StringToUint(string_id, &id);
      DCHECK(result);
      auto compiled_rules = compiled_rules_.find(id);
      if (compiled_rules == compiled_rules_.end() ||
          compiled_rules->second->checksum() != list_checksum.GetString()) {
        ReorganizeRules();
      }
    }
  }

  organized_rules_checksum_ = checksum;

  ApplyOrganizedRules(std::move(*rules), false);
}

void OrganizedRulesManager::OnOrganizedRulesReady(base::Value rules) {
  build_result_ = rules.is_none() ? RuleService::kTooManyAllowRules
                                  : RuleService::kBuildSuccess;
  if (build_result_ != RuleService::kBuildSuccess) {
    organized_rules_changed_callback_.Run(build_result_);
    return;
  }
  ApplyOrganizedRules(std::move(rules), true);
}

void OrganizedRulesManager::Disable() {
  content_rule_list_provider_->InstallContentRuleLists(base::Value::List());
  build_result_ = RuleService::kBuildSuccess;
  organized_rules_checksum_.clear();
  organized_rules_changed_callback_.Run(build_result_);
}

void OrganizedRulesManager::ApplyOrganizedRules(base::Value rules, bool save) {
  DCHECK(rules.is_dict());
  base::Value::List* orgnized_rules =
      rules.GetDict().FindList(rules_json::kOrganizedRules);
  DCHECK(orgnized_rules);
  content_rule_list_provider_->InstallContentRuleLists(*orgnized_rules);

  if (!save) {
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteRulesAndGetChecksum,
                     rules_list_folder_.Append(kOrganizedRulesFileName),
                     std::move(rules)),
      base::BindOnce(
          [](base::WeakPtr<OrganizedRulesManager> self,
             RuleService::IndexBuildResult build_result, std::string checksum) {
            if (self && !checksum.empty()) {
              self->organized_rules_checksum_ = checksum;
              self->organized_rules_changed_callback_.Run(build_result);
            }
          },
          weak_factory_.GetWeakPtr(), build_result_));
}
}  // namespace adblock_filter

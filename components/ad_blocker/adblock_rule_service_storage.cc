// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_rule_service_storage.h"

#include <map>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/ad_blocker/adblock_known_sources_handler.h"
#include "components/ad_blocker/adblock_rule_service.h"

namespace adblock_filter {

namespace {

const char kTrackingRulesKey[] = "tracking-rules";
const char kAdBlockingRulesKey[] = "ad-blocking-rules";
const char kExceptionsTypeKey[] = "exceptions-type";
const char kProcessListKey[] = "process_list";
const char kExemptListKey[] = "exempt_list";
const char kEnabledKey[] = "enabled";
const char kIndexChecksum[] = "index-checksum";

const char kRuleSourcesKey[] = "rule-sources";
const char kKnownSourcesKey[] = "known-sources";
const char kDeletedPresetsKey[] = "deleted-presets";

const char kSourceUrlKey[] = "source-url";
const char kSourceFileKey[] = "source-file";
const char kAllowAbpSnippets[] = "allow-abp-snippets";
const char kNakedHostnameIsPureHost[] = "naked-hostname-is-pure-host";
const char kUseWholeDocumentAllow[] = "use-whole-document-allow";
const char kAllowAttributionTrackerRules[] = "allow-attribution-tracker-rules";
const char kRulesListChecksumKey[] = "rules-list-checksum";
const char kLastUpdateKey[] = "last-upate";
const char kNextFetchKey[] = "next-fetch";
const char kLastFetchResultKey[] = "last-fetch-result";
const char kHasTrackerInfosKey[] = "has-tracker-infos";
const char kValidRulesCountKey[] = "valid-rules-count";
const char kUnsupportedRulesCountKey[] = "unsupported-rules-count";
const char kInvalidRulesCountKey[] = "invalid-rules-count";
const char kTitleKey[] = "title";
const char kHomePageKey[] = "homepage";
const char kLicenseKey[] = "license";
const char kRedirectKey[] = "redirect";
const char kVersionKey[] = "version";
const char kExpiresKey[] = "expires";

const char kBlockedDomainsCountersKey[] = "blocked-domain-counters";
const char kBlockedForOriginCountersKey[] = "blocked-for-origin-counters";
const char kBlockedReportingStartKey[] = "blocked-reporting-start";

const char kPresetIdKey[] = "preset-id";

const int kCurrentStorageVersion = 10;

const base::FilePath::CharType kSourcesFileName[] =
    FILE_PATH_LITERAL("AdBlockState");

// Extension used for backup files (copy of main file created during startup).
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// How often we save.
constexpr base::TimeDelta kSaveDelay = base::Seconds(2);

void BackupCallback(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

std::map<std::string, int> LoadCounters(const base::Value& counters_value) {
  std::map<std::string, int> counters;
  DCHECK(counters_value.is_dict());

  for (const auto [counter, value] : counters_value.GetDict()) {
    if (!value.is_int())
      continue;
    counters[counter] = value.GetInt();
  }

  return counters;
}

std::optional<RuleSourceCore> LoadRuleSourceCore(
    base::Value::Dict& source_dict) {
  const std::string* source_url_string = source_dict.FindString(kSourceUrlKey);
  const std::string* source_file = source_dict.FindString(kSourceFileKey);

  std::optional<RuleSourceCore> core;
  if (source_url_string) {
    core = RuleSourceCore::FromUrl(GURL(*source_url_string));
  } else if (source_file) {
    core =
        RuleSourceCore::FromFile(base::FilePath::FromUTF8Unsafe(*source_file));
  }

  if (!core) {
    return core;
  }

  RuleSourceSettings settings;

  std::optional<bool> allow_abp_snippets =
      source_dict.FindBool(kAllowAbpSnippets);
  settings.allow_abp_snippets =
      allow_abp_snippets && allow_abp_snippets.value();

  std::optional<bool> naked_hostname_is_pure_host =
      source_dict.FindBool(kNakedHostnameIsPureHost);
  // Enabled by default.
  settings.naked_hostname_is_pure_host =
      !naked_hostname_is_pure_host || naked_hostname_is_pure_host.value();

  std::optional<bool> use_whole_document_allow =
      source_dict.FindBool(kUseWholeDocumentAllow);
  // Enabled by default.
  settings.use_whole_document_allow =
      !use_whole_document_allow || use_whole_document_allow.value();

  std::optional<bool> allow_attribution_tracker_rules =
      source_dict.FindBool(kAllowAttributionTrackerRules);
  settings.allow_attribution_tracker_rules =
      allow_attribution_tracker_rules &&
      allow_attribution_tracker_rules.value();

  core->set_settings(settings);

  return core;
}

ActiveRuleSources LoadSourcesList(base::Value::List& sources_list) {
  ActiveRuleSources rule_sources;

  for (auto& source_value : sources_list) {
    if (!source_value.is_dict())
      continue;

    auto& source_dict = source_value.GetDict();

    std::optional<RuleSourceCore> core = LoadRuleSourceCore(source_dict);

    if (!core) {
      continue;
    }

    rule_sources.emplace_back(std::move(*core));

    std::string* rules_list_checksum =
        source_dict.FindString(kRulesListChecksumKey);
    if (rules_list_checksum)
      rule_sources.back().rules_list_checksum = std::move(*rules_list_checksum);

    std::optional<base::Time> last_update =
        base::ValueToTime(source_dict.Find(kLastUpdateKey));
    if (last_update) {
      rule_sources.back().last_update = last_update.value();
    }

    std::optional<base::Time> next_fetch =
        base::ValueToTime(source_dict.Find(kNextFetchKey));
    if (next_fetch) {
      rule_sources.back().next_fetch = next_fetch.value();
    }

    std::optional<int> last_fetch_result =
        source_dict.FindInt(kLastFetchResultKey);
    if (last_fetch_result &&
        last_fetch_result.value() >= static_cast<int>(FetchResult::kFirst) &&
        last_fetch_result.value() <= static_cast<int>(FetchResult::kLast))
      rule_sources.back().last_fetch_result =
          FetchResult(last_fetch_result.value());

    std::optional<bool> has_tracker_infos =
        source_dict.FindBool(kHasTrackerInfosKey);
    if (has_tracker_infos)
      rule_sources.back().has_tracker_infos = has_tracker_infos.value();

    std::optional<int> valid_rules_count =
        source_dict.FindInt(kValidRulesCountKey);
    if (valid_rules_count)
      rule_sources.back().rules_info.valid_rules = *valid_rules_count;

    std::optional<int> unsupported_rules_count =
        source_dict.FindInt(kUnsupportedRulesCountKey);
    if (unsupported_rules_count)
      rule_sources.back().rules_info.unsupported_rules =
          *unsupported_rules_count;

    std::optional<int> invalid_rules_count =
        source_dict.FindInt(kInvalidRulesCountKey);
    if (invalid_rules_count)
      rule_sources.back().rules_info.invalid_rules = *invalid_rules_count;

    std::string* title = source_dict.FindString(kTitleKey);
    if (title)
      rule_sources.back().unsafe_adblock_metadata.title = std::move(*title);

    const std::string* homepage = source_dict.FindString(kHomePageKey);
    if (homepage)
      rule_sources.back().unsafe_adblock_metadata.homepage = GURL(*homepage);

    const std::string* license = source_dict.FindString(kLicenseKey);
    if (license)
      rule_sources.back().unsafe_adblock_metadata.license = GURL(*license);

    const std::string* redirect = source_dict.FindString(kRedirectKey);
    if (redirect)
      rule_sources.back().unsafe_adblock_metadata.redirect = GURL(*redirect);

    std::optional<int64_t> version =
        base::ValueToInt64(source_dict.Find(kVersionKey));
    if (version)
      rule_sources.back().unsafe_adblock_metadata.version = *version;

    std::optional<base::TimeDelta> expires =
        base::ValueToTimeDelta(source_dict.Find(kExpiresKey));
    if (last_update) {
      rule_sources.back().unsafe_adblock_metadata.expires = expires.value();
    }
  }

  return rule_sources;
}

std::set<std::string> LoadStringSetFromList(base::Value::List& list) {
  std::set<std::string> string_set;
  for (auto& item : list) {
    if (!item.is_string())
      continue;
    string_set.insert(std::move(item.GetString()));
  }

  return string_set;
}

std::vector<KnownRuleSource> LoadKnownSources(base::Value::List& sources_list) {
  std::vector<KnownRuleSource> known_sources;

  for (auto& source_value : sources_list) {
    if (!source_value.is_dict())
      continue;

    auto& source_dict = source_value.GetDict();

    std::optional<RuleSourceCore> core = LoadRuleSourceCore(source_dict);

    if (!core) {
      continue;
    }

    known_sources.emplace_back(std::move(*core));

    std::string* preset_id = source_dict.FindString(kPresetIdKey);
    if (preset_id)
      known_sources.back().preset_id = std::move(*preset_id);
  }

  return known_sources;
}

void LoadRulesGroup(RuleGroup group,
                    base::Value::Dict& rule_group_dict,
                    RuleServiceStorage::LoadResult& load_result) {
  std::optional<int> active_exception_list =
      rule_group_dict.FindInt(kExceptionsTypeKey);
  if (active_exception_list &&
      active_exception_list >= RuleManager::kFirstExceptionList &&
      active_exception_list <= RuleManager::kLastExceptionList) {
    load_result.active_exceptions_lists[static_cast<size_t>(group)] =
        RuleManager::ExceptionsList(active_exception_list.value());
  }

  base::Value::List* process_list = rule_group_dict.FindList(kProcessListKey);
  if (process_list)
    load_result
        .exceptions[static_cast<size_t>(group)][RuleManager::kProcessList] =
        LoadStringSetFromList(*process_list);

  base::Value::List* exempt_list = rule_group_dict.FindList(kExemptListKey);
  if (exempt_list)
    load_result
        .exceptions[static_cast<size_t>(group)][RuleManager::kExemptList] =
        LoadStringSetFromList(*exempt_list);

  std::optional<bool> enabled = rule_group_dict.FindBool(kEnabledKey);
  if (enabled)
    load_result.groups_enabled[static_cast<size_t>(group)] = enabled.value();

  std::string* index_checksum = rule_group_dict.FindString(kIndexChecksum);
  if (index_checksum)
    load_result.index_checksums[static_cast<size_t>(group)] =
        std::move(*index_checksum);

  base::Value::List* sources_list = rule_group_dict.FindList(kRuleSourcesKey);
  if (sources_list)
    load_result.rule_sources[static_cast<size_t>(group)] =
        LoadSourcesList(*sources_list);

  base::Value::List* known_sources_list =
      rule_group_dict.FindList(kKnownSourcesKey);
  if (known_sources_list)
    load_result.known_sources[static_cast<size_t>(group)] =
        LoadKnownSources(*known_sources_list);

  base::Value::List* deleted_presets_list =
      rule_group_dict.FindList(kDeletedPresetsKey);
  if (deleted_presets_list)
    load_result.deleted_presets[static_cast<size_t>(group)] =
        LoadStringSetFromList(*deleted_presets_list);

  base::Value* blocked_domains_counters =
      rule_group_dict.Find(kBlockedDomainsCountersKey);
  if (blocked_domains_counters)
    load_result.blocked_domains_counters[static_cast<size_t>(group)] =
        LoadCounters(*blocked_domains_counters);

  base::Value* blocked_for_origin_counters =
      rule_group_dict.Find(kBlockedForOriginCountersKey);
  if (blocked_for_origin_counters)
    load_result.blocked_for_origin_counters[static_cast<size_t>(group)] =
        LoadCounters(*blocked_for_origin_counters);
}

RuleServiceStorage::LoadResult DoLoad(const base::FilePath& path) {
  RuleServiceStorage::LoadResult load_result;

  JSONFileValueDeserializer serializer(path);
  std::unique_ptr<base::Value> root(serializer.Deserialize(nullptr, nullptr));

  if (root.get() && root->is_dict()) {
    base::Value::Dict* tracking_rules =
        root->GetDict().FindDict(kTrackingRulesKey);
    if (tracking_rules) {
      LoadRulesGroup(RuleGroup::kTrackingRules, *tracking_rules, load_result);
    }
    base::Value::Dict* ad_blocking_rules =
        root->GetDict().FindDict(kAdBlockingRulesKey);
    if (ad_blocking_rules) {
      LoadRulesGroup(RuleGroup::kAdBlockingRules, *ad_blocking_rules,
                     load_result);
    }

    std::optional<base::Time> blocked_reporting_start =
        base::ValueToTime(root->GetDict().Find(kBlockedReportingStartKey));
    if (blocked_reporting_start)
      load_result.blocked_reporting_start = *blocked_reporting_start;

    std::optional<int> version = root->GetDict().FindInt(kVersionKey);
    if (version)
      load_result.storage_version =
          std::max(0, std::min(kCurrentStorageVersion, version.value()));
  }

  return load_result;
}

base::Value SerializeCounters(const std::map<std::string, int>& counters) {
  base::Value::Dict buffer;
  for (const auto& [counter, value] : counters) {
    buffer.Set(counter, value);
  }
  return base::Value(std::move(buffer));
}

base::Value::Dict SerializeRuleCore(const RuleSourceCore& core) {
  base::Value::Dict core_dict;

  if (core.is_from_url())
    core_dict.Set(kSourceUrlKey, core.source_url().spec());
  else
    core_dict.Set(kSourceFileKey, core.source_file().AsUTF8Unsafe());

  core_dict.Set(kAllowAbpSnippets, core.settings().allow_abp_snippets);
  core_dict.Set(kNakedHostnameIsPureHost,
                core.settings().naked_hostname_is_pure_host);
  core_dict.Set(kUseWholeDocumentAllow,
                core.settings().use_whole_document_allow);
  core_dict.Set(kAllowAttributionTrackerRules,
                core.settings().allow_attribution_tracker_rules);

  return core_dict;
}

base::Value::List SerializeSourcesList(
    const std::map<uint32_t, ActiveRuleSource>& rule_sources) {
  base::Value::List sources_list;
  for (const auto& [id, rule_source] : rule_sources) {
    base::Value::Dict source_dict = SerializeRuleCore(rule_source.core);
    source_dict.Set(kRulesListChecksumKey, rule_source.rules_list_checksum);
    source_dict.Set(kLastUpdateKey, base::TimeToValue(rule_source.last_update));
    source_dict.Set(kNextFetchKey, base::TimeToValue(rule_source.next_fetch));
    source_dict.Set(kValidRulesCountKey, rule_source.rules_info.valid_rules);
    source_dict.Set(kUnsupportedRulesCountKey,
                    rule_source.rules_info.unsupported_rules);
    source_dict.Set(kInvalidRulesCountKey,
                    rule_source.rules_info.invalid_rules);
    source_dict.Set(kLastFetchResultKey,
                    static_cast<int>(rule_source.last_fetch_result));
    source_dict.Set(kHasTrackerInfosKey, rule_source.has_tracker_infos);
    source_dict.Set(kTitleKey, rule_source.unsafe_adblock_metadata.title);
    source_dict.Set(
        kHomePageKey,
        rule_source.unsafe_adblock_metadata.homepage.possibly_invalid_spec());
    source_dict.Set(
        kLicenseKey,
        rule_source.unsafe_adblock_metadata.license.possibly_invalid_spec());
    source_dict.Set(
        kRedirectKey,
        rule_source.unsafe_adblock_metadata.redirect.possibly_invalid_spec());
    source_dict.Set(
        kVersionKey,
        base::Int64ToValue(rule_source.unsafe_adblock_metadata.version));
    source_dict.Set(
        kExpiresKey,
        base::TimeDeltaToValue(rule_source.unsafe_adblock_metadata.expires));
    sources_list.Append(std::move(source_dict));
  }

  return sources_list;
}

base::Value SerializeStringSetToList(const std::set<std::string>& string_set) {
  base::Value list(base::Value::Type::LIST);
  for (const auto& item : string_set) {
    list.GetList().Append(item);
  }

  return list;
}

base::Value::List SerializeKnownSourcesList(
    const KnownRuleSources& rule_sources) {
  base::Value::List sources_list;
  for (const auto& [id, rule_source] : rule_sources) {
    if (!rule_source.removable)
      continue;

    base::Value::Dict source = SerializeRuleCore(rule_source.core);
    if (!rule_source.preset_id.empty())
      source.Set(kPresetIdKey, rule_source.preset_id);
    sources_list.Append(std::move(source));
  }

  return sources_list;
}

base::Value::Dict SerializeRuleGroup(RuleService* service, RuleGroup group) {
  base::Value::Dict rule_group;
  rule_group.Set(kExceptionsTypeKey,
                 service->GetRuleManager()->GetActiveExceptionList(group));
  rule_group.Set(kProcessListKey, SerializeStringSetToList(
                                      service->GetRuleManager()->GetExceptions(
                                          group, RuleManager::kProcessList)));
  rule_group.Set(kExemptListKey, SerializeStringSetToList(
                                     service->GetRuleManager()->GetExceptions(
                                         group, RuleManager::kExemptList)));
  rule_group.Set(kEnabledKey, service->IsRuleGroupEnabled(group));

  rule_group.Set(
      kRuleSourcesKey,
      SerializeSourcesList(service->GetRuleManager()->GetRuleSources(group)));
  rule_group.Set(kKnownSourcesKey,
                 SerializeKnownSourcesList(
                     service->GetKnownSourcesHandler()->GetSources(group)));
  rule_group.Set(
      kDeletedPresetsKey,
      SerializeStringSetToList(
          service->GetKnownSourcesHandler()->GetDeletedPresets(group)));
  rule_group.Set(kIndexChecksum, service->GetRulesIndexChecksum(group));

  if (service->GetStateAndLogs()) {
    rule_group.Set(
        kBlockedDomainsCountersKey,
        SerializeCounters(
            service->GetStateAndLogs()
                ->GetBlockedDomainCounters()[static_cast<size_t>(group)]));

    rule_group.Set(
        kBlockedForOriginCountersKey,
        SerializeCounters(
            service->GetStateAndLogs()
                ->GetBlockedForOriginCounters()[static_cast<size_t>(group)]));
  }

  return rule_group;
}

}  // namespace

RuleServiceStorage::LoadResult::LoadResult() = default;
RuleServiceStorage::LoadResult::~LoadResult() = default;
RuleServiceStorage::LoadResult::LoadResult(LoadResult&& load_result) = default;
RuleServiceStorage::LoadResult& RuleServiceStorage::LoadResult::operator=(
    LoadResult&& load_result) = default;

RuleServiceStorage::RuleServiceStorage(
    const base::FilePath& profile_path,
    RuleService* rule_service,
    scoped_refptr<base::SequencedTaskRunner> file_io_task_runner)
    : file_io_task_runner_(std::move(file_io_task_runner)),
      rule_service_(rule_service),
      writer_(profile_path.Append(kSourcesFileName),
              file_io_task_runner_,
              kSaveDelay),
      weak_factory_(this) {
  DCHECK(rule_service_);
  file_io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BackupCallback, writer_.path()));
}

RuleServiceStorage::~RuleServiceStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void RuleServiceStorage::Load(LoadingDoneCallback loading_done_callback) {
  loading_done_callback_ = std::move(loading_done_callback);
  file_io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoLoad, writer_.path()),
      base::BindOnce(&RuleServiceStorage::OnLoadFinished,
                     weak_factory_.GetWeakPtr()));
}

void RuleServiceStorage::OnLoadFinished(LoadResult load_result) {
  std::move(loading_done_callback_).Run(std::move(load_result));
}

void RuleServiceStorage::ScheduleSave() {
  writer_.ScheduleWrite(this);
}

void RuleServiceStorage::OnRuleServiceShutdown() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

std::optional<std::string> RuleServiceStorage::SerializeData() {
  base::Value::Dict root;

  root.Set(kTrackingRulesKey,
           SerializeRuleGroup(rule_service_, RuleGroup::kTrackingRules));
  root.Set(kAdBlockingRulesKey,
           SerializeRuleGroup(rule_service_, RuleGroup::kAdBlockingRules));
  if (rule_service_->GetStateAndLogs()) {
    root.Set(kBlockedReportingStartKey,
             base::TimeToValue(
                 rule_service_->GetStateAndLogs()->GetBlockedCountersStart()));
  }
  root.Set(kVersionKey, kCurrentStorageVersion);

  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(root))
    return std::nullopt;

  return output;
}

}  // namespace adblock_filter

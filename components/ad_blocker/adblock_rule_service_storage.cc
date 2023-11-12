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
const char kGroupKey[] = "group";
const char kAllowAbpSnippets[] = "allow-abp-snippets";
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

const int kCurrentStorageVersion = 6;

const base::FilePath::CharType kSourcesFileName[] =
    FILE_PATH_LITERAL("AdBlockState");

// Extension used for backup files (copy of main file created during startup).
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// How often we save.
const int kSaveDelay = 10;  // seconds

void BackupCallback(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

void LoadCounters(const base::Value& counters_value,
                  std::map<std::string, int>& counters) {
  DCHECK(counters_value.is_dict());

  for (const auto counter : counters_value.GetDict()) {
    if (!counter.second.is_int())
      continue;
    counters[counter.first] = counter.second.GetInt();
  }
}

void LoadSourcesList(base::Value::List& sources_list, RuleSources& rule_sources) {
  for (auto& source_value : sources_list) {
    if (!source_value.is_dict())
      continue;

    const std::string* source_file = source_value.GetDict().FindString(kSourceFileKey);
    const std::string* source_url_string =
        source_value.GetDict().FindString(kSourceUrlKey);

    absl::optional<int> group = source_value.GetDict().FindInt(kGroupKey);
    // The rule must have its group set
    if (!group || group.value() < static_cast<int>(RuleGroup::kFirst) ||
        group.value() > static_cast<int>(RuleGroup::kLast))
      continue;

    GURL source_url;
    if (source_url_string) {
      source_url = GURL(*source_url_string);
      if (!source_url.is_valid() || source_url.is_empty())
        continue;
      rule_sources.emplace_back(source_url, RuleGroup(group.value()));
    } else if (source_file) {
      rule_sources.emplace_back(base::FilePath::FromUTF8Unsafe(*source_file),
                                RuleGroup(group.value()));
    } else {
      // The rules must either come form a file or a URL
      continue;
    }

    absl::optional<bool> allow_abp_snippets =
        source_value.GetDict().FindBool(kAllowAbpSnippets);
    if (allow_abp_snippets && allow_abp_snippets.value())
      rule_sources.back().allow_abp_snippets = true;

    std::string* rules_list_checksum =
        source_value.GetDict().FindString(kRulesListChecksumKey);
    if (rules_list_checksum)
      rule_sources.back().rules_list_checksum = std::move(*rules_list_checksum);

    absl::optional<base::Time> last_update =
        base::ValueToTime(source_value.GetDict().Find(kLastUpdateKey));
    if (last_update) {
      rule_sources.back().last_update = last_update.value();
    }

    absl::optional<base::Time> next_fetch =
        base::ValueToTime(source_value.GetDict().Find(kNextFetchKey));
    if (next_fetch) {
      rule_sources.back().next_fetch = next_fetch.value();
    }

    absl::optional<int> last_fetch_result =
        source_value.GetDict().FindInt(kLastFetchResultKey);
    if (last_fetch_result &&
        last_fetch_result.value() >= static_cast<int>(FetchResult::kFirst) &&
        last_fetch_result.value() <= static_cast<int>(FetchResult::kLast))
      rule_sources.back().last_fetch_result =
          FetchResult(last_fetch_result.value());

    absl::optional<bool> has_tracker_infos =
        source_value.GetDict().FindBool(kHasTrackerInfosKey);
    if (has_tracker_infos)
      rule_sources.back().has_tracker_infos = has_tracker_infos.value();

    absl::optional<int> valid_rules_count =
        source_value.GetDict().FindInt(kValidRulesCountKey);
    if (valid_rules_count)
      rule_sources.back().rules_info.valid_rules = *valid_rules_count;

    absl::optional<int> unsupported_rules_count =
        source_value.GetDict().FindInt(kUnsupportedRulesCountKey);
    if (unsupported_rules_count)
      rule_sources.back().rules_info.unsupported_rules =
          *unsupported_rules_count;

    absl::optional<int> invalid_rules_count =
        source_value.GetDict().FindInt(kInvalidRulesCountKey);
    if (invalid_rules_count)
      rule_sources.back().rules_info.invalid_rules = *invalid_rules_count;

    std::string* title = source_value.GetDict().FindString(kTitleKey);
    if (title)
      rule_sources.back().unsafe_adblock_metadata.title = std::move(*title);

    const std::string* homepage = source_value.GetDict().FindString(kHomePageKey);
    if (homepage)
      rule_sources.back().unsafe_adblock_metadata.homepage = GURL(*homepage);

    const std::string* license = source_value.GetDict().FindString(kLicenseKey);
    if (license)
      rule_sources.back().unsafe_adblock_metadata.license = GURL(*license);

    const std::string* redirect = source_value.GetDict().FindString(kRedirectKey);
    if (redirect)
      rule_sources.back().unsafe_adblock_metadata.redirect = GURL(*redirect);

    absl::optional<int64_t> version =
        base::ValueToInt64(source_value.GetDict().Find(kVersionKey));
    if (version)
      rule_sources.back().unsafe_adblock_metadata.version = *version;

    absl::optional<base::TimeDelta> expires =
        base::ValueToTimeDelta(source_value.GetDict().Find(kExpiresKey));
    if (last_update) {
      rule_sources.back().unsafe_adblock_metadata.expires = expires.value();
    }
  }
}

void LoadStringSetFromList(base::Value::List& list,
                           std::set<std::string>& string_set) {
  for (auto& item : list) {
    if (!item.is_string())
      continue;
    string_set.insert(std::move(item.GetString()));
  }
}

void LoadKnownSources(base::Value::List& sources_list,
                      std::vector<KnownRuleSource>& known_sources) {
  for (auto& source_value : sources_list) {
    if (!source_value.is_dict())
      continue;

    const std::string* source_file = source_value.GetDict().FindString(kSourceFileKey);
    const std::string* source_url_string =
        source_value.GetDict().FindString(kSourceUrlKey);

    absl::optional<int> group = source_value.GetDict().FindInt(kGroupKey);
    // The rule must have its group set
    if (!group || group.value() < static_cast<int>(RuleGroup::kFirst) ||
        group.value() > static_cast<int>(RuleGroup::kLast))
      continue;

    GURL source_url;
    if (source_url_string) {
      source_url = GURL(*source_url_string);
      if (!source_url.is_valid() || source_url.is_empty())
        continue;
      known_sources.emplace_back(source_url, RuleGroup(group.value()));
    } else if (source_file) {
      known_sources.emplace_back(base::FilePath::FromUTF8Unsafe(*source_file),
                                 RuleGroup(group.value()));
    } else {
      // The rules must either come form a file or a URL
      continue;
    }

    absl::optional<bool> allow_abp_snippets =
        source_value.GetDict().FindBool(kAllowAbpSnippets);
    if (allow_abp_snippets && allow_abp_snippets.value())
      known_sources.back().allow_abp_snippets = true;

    std::string* preset_id = source_value.GetDict().FindString(kPresetIdKey);
    if (preset_id)
      known_sources.back().preset_id = std::move(*preset_id);
  }
}

void LoadRulesGroup(RuleGroup group,
                    base::Value& rule_group_value,
                    RuleServiceStorage::LoadResult& load_result) {
  DCHECK(rule_group_value.is_dict());
  absl::optional<int> active_exception_list =
      rule_group_value.GetDict().FindInt(kExceptionsTypeKey);
  if (active_exception_list &&
      active_exception_list >= RuleManager::kFirstExceptionList &&
      active_exception_list <= RuleManager::kLastExceptionList) {
    load_result.active_exceptions_lists[static_cast<size_t>(group)] =
        RuleManager::ExceptionsList(active_exception_list.value());
  }

  base::Value::List* process_list = rule_group_value.GetDict().FindList(kProcessListKey);
  if (process_list)
    LoadStringSetFromList(*process_list,
                          load_result.exceptions[static_cast<size_t>(group)]
                                                [RuleManager::kProcessList]);

  base::Value::List* exempt_list = rule_group_value.GetDict().FindList(kExemptListKey);
  if (exempt_list)
    LoadStringSetFromList(*exempt_list,
                          load_result.exceptions[static_cast<size_t>(group)]
                                                [RuleManager::kExemptList]);

  absl::optional<bool> enabled = rule_group_value.GetDict().FindBool(kEnabledKey);
  if (enabled)
    load_result.groups_enabled[static_cast<size_t>(group)] = enabled.value();

  std::string* index_checksum = rule_group_value.GetDict().FindString(kIndexChecksum);
  if (index_checksum)
    load_result.index_checksums[static_cast<size_t>(group)] =
        std::move(*index_checksum);

  base::Value::List* sources_list = rule_group_value.GetDict().FindList(kRuleSourcesKey);
  if (sources_list)
    LoadSourcesList(*sources_list,
                    load_result.rule_sources[static_cast<size_t>(group)]);

  base::Value::List* known_sources_list =
      rule_group_value.GetDict().FindList(kKnownSourcesKey);
  if (known_sources_list)
    LoadKnownSources(*known_sources_list,
                     load_result.known_sources[static_cast<size_t>(group)]);

  base::Value::List* deleted_presets_list =
      rule_group_value.GetDict().FindList(kDeletedPresetsKey);
  if (deleted_presets_list)
    LoadStringSetFromList(
        *deleted_presets_list,
        load_result.deleted_presets[static_cast<size_t>(group)]);

  base::Value* blocked_domains_counters =
      rule_group_value.GetDict().Find(kBlockedDomainsCountersKey);
  if (blocked_domains_counters)
    LoadCounters(
        *blocked_domains_counters,
        load_result.blocked_domains_counters[static_cast<size_t>(group)]);

  base::Value* blocked_for_origin_counters =
      rule_group_value.GetDict().Find(kBlockedForOriginCountersKey);
  if (blocked_for_origin_counters)
    LoadCounters(
        *blocked_for_origin_counters,
        load_result.blocked_for_origin_counters[static_cast<size_t>(group)]);
}

RuleServiceStorage::LoadResult DoLoad(const base::FilePath& path) {
  RuleServiceStorage::LoadResult load_result;

  JSONFileValueDeserializer serializer(path);
  std::unique_ptr<base::Value> root(serializer.Deserialize(nullptr, nullptr));

  if (root.get() && root->is_dict()) {
    base::Value* tracking_rules = root->GetDict().Find(kTrackingRulesKey);
    if (tracking_rules) {
      LoadRulesGroup(RuleGroup::kTrackingRules, *tracking_rules, load_result);
    }
    base::Value* ad_blocking_rules = root->GetDict().Find(kAdBlockingRulesKey);
    if (ad_blocking_rules) {
      LoadRulesGroup(RuleGroup::kAdBlockingRules, *ad_blocking_rules,
                     load_result);
    }

    absl::optional<base::Time> blocked_reporting_start =
        base::ValueToTime(root->GetDict().Find(kBlockedReportingStartKey));
    if (blocked_reporting_start)
      load_result.blocked_reporting_start = *blocked_reporting_start;

    absl::optional<int> version = root->GetDict().FindInt(kVersionKey);
    if (version)
      load_result.storage_version =
          std::max(0, std::min(kCurrentStorageVersion, version.value()));
  }

  return load_result;
}

base::Value SerializeCounters(const std::map<std::string, int>& counters) {
  base::Value::Dict buffer;
  for (const auto& counter : counters) {
    buffer.Set(counter.first, counter.second);
  }
  return base::Value(std::move(buffer));
}

base::Value::List SerializeSourcesList(
    const std::map<uint32_t, RuleSource>& rule_sources) {
  base::Value::List sources_list;
  for (const auto& map_entry : rule_sources) {
    const RuleSource& rule_source = map_entry.second;
    base::Value::Dict source_dict;
    if (rule_source.is_from_url)
      source_dict.Set(kSourceUrlKey, rule_source.source_url.spec());
    else
      source_dict.Set(kSourceFileKey, rule_source.source_file.AsUTF8Unsafe());
    source_dict.Set(kGroupKey, static_cast<int>(rule_source.group));
    source_dict.Set(kAllowAbpSnippets, rule_source.allow_abp_snippets);
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
    source_dict.Set(kHomePageKey,
                    rule_source.unsafe_adblock_metadata.homepage.spec());
    source_dict.Set(kLicenseKey,
                    rule_source.unsafe_adblock_metadata.license.spec());
    source_dict.Set(kRedirectKey,
                    rule_source.unsafe_adblock_metadata.redirect.spec());
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
  for (const auto& map_entry : rule_sources) {
    const KnownRuleSource& rule_source = map_entry.second;
    if (!rule_source.removable)
      continue;

    base::Value::Dict source;
    if (rule_source.is_from_url)
      source.Set(kSourceUrlKey, rule_source.source_url.spec());
    else
      source.Set(kSourceFileKey, rule_source.source_file.AsUTF8Unsafe());
    source.Set(kGroupKey, static_cast<int>(rule_source.group));
    source.Set(kAllowAbpSnippets, rule_source.allow_abp_snippets);
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

  if (service->GetBlockerUrlsReporter()) {
    rule_group.Set(kBlockedDomainsCountersKey,
                   SerializeCounters(
                       service->GetBlockerUrlsReporter()
                           ->GetBlockedDomains()[static_cast<size_t>(group)]));

    rule_group.Set(
        kBlockedForOriginCountersKey,
        SerializeCounters(
            service->GetBlockerUrlsReporter()
                ->GetBlockedForOrigin()[static_cast<size_t>(group)]));
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
              base::Seconds(kSaveDelay)),
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

absl::optional<std::string> RuleServiceStorage::SerializeData() {
  base::Value::Dict root;

  root.Set(kTrackingRulesKey,
           SerializeRuleGroup(rule_service_, RuleGroup::kTrackingRules));
  root.Set(kAdBlockingRulesKey,
           SerializeRuleGroup(rule_service_, RuleGroup::kAdBlockingRules));
  if (rule_service_->GetBlockerUrlsReporter()) {
    root.Set(kBlockedReportingStartKey,
             base::TimeToValue(
                 rule_service_->GetBlockerUrlsReporter()->GetReportingStart()));
  }
  root.Set(kVersionKey, kCurrentStorageVersion);

  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(root))
    return absl::nullopt;

  return output;
}

}  // namespace adblock_filter

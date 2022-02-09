// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rule_service_storage.h"

#include <map>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/values_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/request_filter/adblock_filter/adblock_known_sources_handler.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

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

const char kCountersKey[] = "counters";

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

  for (const auto counter : counters_value.DictItems()) {
    if (!counter.second.is_int())
      continue;
    counters[counter.first] = counter.second.GetInt();
  }
}

void LoadSourcesList(base::Value& sources_list, RuleSources& rule_sources) {
  DCHECK(sources_list.is_list());
  for (auto& source_value : sources_list.GetList()) {
    if (!source_value.is_dict())
      continue;

    const std::string* source_file = source_value.FindStringKey(kSourceFileKey);
    const std::string* source_url_string =
        source_value.FindStringKey(kSourceUrlKey);

    absl::optional<int> group = source_value.FindIntKey(kGroupKey);
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
        source_value.FindBoolKey(kAllowAbpSnippets);
    if (allow_abp_snippets && allow_abp_snippets.value())
      rule_sources.back().allow_abp_snippets = true;

    std::string* rules_list_checksum =
        source_value.FindStringKey(kRulesListChecksumKey);
    if (rules_list_checksum)
      rule_sources.back().rules_list_checksum = std::move(*rules_list_checksum);

    absl::optional<base::Time> last_update =
        base::ValueToTime(source_value.FindKey(kLastUpdateKey));
    if (last_update) {
      rule_sources.back().last_update = last_update.value();
    }

    absl::optional<base::Time> next_fetch =
        base::ValueToTime(source_value.FindKey(kNextFetchKey));
    if (next_fetch) {
      rule_sources.back().next_fetch = next_fetch.value();
    }

    absl::optional<int> last_fetch_result =
        source_value.FindIntKey(kLastFetchResultKey);
    if (last_fetch_result &&
        last_fetch_result.value() >= static_cast<int>(FetchResult::kFirst) &&
        last_fetch_result.value() <= static_cast<int>(FetchResult::kLast))
      rule_sources.back().last_fetch_result =
          FetchResult(last_fetch_result.value());

    absl::optional<bool> has_tracker_infos =
        source_value.FindBoolKey(kHasTrackerInfosKey);
    if (has_tracker_infos)
      rule_sources.back().has_tracker_infos = has_tracker_infos.value();

    absl::optional<int> valid_rules_count =
        source_value.FindIntKey(kValidRulesCountKey);
    if (valid_rules_count)
      rule_sources.back().rules_info.valid_rules = *valid_rules_count;

    absl::optional<int> unsupported_rules_count =
        source_value.FindIntKey(kUnsupportedRulesCountKey);
    if (unsupported_rules_count)
      rule_sources.back().rules_info.unsupported_rules =
          *unsupported_rules_count;

    absl::optional<int> invalid_rules_count =
        source_value.FindIntKey(kInvalidRulesCountKey);
    if (invalid_rules_count)
      rule_sources.back().rules_info.invalid_rules = *invalid_rules_count;

    std::string* title = source_value.FindStringKey(kTitleKey);
    if (title)
      rule_sources.back().unsafe_adblock_metadata.title = std::move(*title);

    const std::string* homepage = source_value.FindStringKey(kHomePageKey);
    if (homepage)
      rule_sources.back().unsafe_adblock_metadata.homepage = GURL(*homepage);

    const std::string* license = source_value.FindStringKey(kLicenseKey);
    if (license)
      rule_sources.back().unsafe_adblock_metadata.license = GURL(*license);

    const std::string* redirect = source_value.FindStringKey(kRedirectKey);
    if (redirect)
      rule_sources.back().unsafe_adblock_metadata.redirect = GURL(*redirect);

    absl::optional<int64_t> version =
        base::ValueToInt64(source_value.FindKey(kVersionKey));
    if (version)
      rule_sources.back().unsafe_adblock_metadata.version = *version;

    absl::optional<base::TimeDelta> expires =
        base::ValueToTimeDelta(source_value.FindKey(kExpiresKey));
    if (last_update) {
      rule_sources.back().unsafe_adblock_metadata.expires = expires.value();
    }
  }
}

void LoadStringSetFromList(base::Value& list,
                           std::set<std::string>& string_set) {
  DCHECK(list.is_list());
  for (auto& item : list.GetList()) {
    if (!item.is_string())
      continue;
    string_set.insert(std::move(item.GetString()));
  }
}

void LoadKnownSources(base::Value& sources_list,
                      std::vector<KnownRuleSource>& known_sources) {
  DCHECK(sources_list.is_list());
  for (auto& source_value : sources_list.GetList()) {
    if (!source_value.is_dict())
      continue;

    const std::string* source_file = source_value.FindStringKey(kSourceFileKey);
    const std::string* source_url_string =
        source_value.FindStringKey(kSourceUrlKey);

    absl::optional<int> group = source_value.FindIntKey(kGroupKey);
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
        source_value.FindBoolKey(kAllowAbpSnippets);
    if (allow_abp_snippets && allow_abp_snippets.value())
      known_sources.back().allow_abp_snippets = true;

    std::string* preset_id = source_value.FindStringKey(kPresetIdKey);
    if (preset_id)
      known_sources.back().preset_id = std::move(*preset_id);
  }
}

void LoadRulesGroup(RuleGroup group,
                    base::Value& rule_group_value,
                    RuleServiceStorage::LoadResult& load_result) {
  DCHECK(rule_group_value.is_dict());
  absl::optional<int> active_exception_list =
      rule_group_value.FindIntKey(kExceptionsTypeKey);
  if (active_exception_list &&
      active_exception_list >= RuleService::kFirstExceptionList &&
      active_exception_list <= RuleService::kLastExceptionList) {
    load_result.active_exceptions_lists[static_cast<size_t>(group)] =
        RuleService::ExceptionsList(active_exception_list.value());
  }

  base::Value* process_list = rule_group_value.FindListKey(kProcessListKey);
  if (process_list)
    LoadStringSetFromList(*process_list,
                          load_result.exceptions[static_cast<size_t>(group)]
                                                [RuleService::kProcessList]);

  base::Value* exempt_list = rule_group_value.FindListKey(kExemptListKey);
  if (exempt_list)
    LoadStringSetFromList(*exempt_list,
                          load_result.exceptions[static_cast<size_t>(group)]
                                                [RuleService::kExemptList]);

  absl::optional<bool> enabled = rule_group_value.FindBoolKey(kEnabledKey);
  if (enabled)
    load_result.groups_enabled[static_cast<size_t>(group)] = enabled.value();

  std::string* index_checksum = rule_group_value.FindStringKey(kIndexChecksum);
  if (index_checksum)
    load_result.index_checksums[static_cast<size_t>(group)] =
        std::move(*index_checksum);

  base::Value* sources_list = rule_group_value.FindListKey(kRuleSourcesKey);
  if (sources_list)
    LoadSourcesList(*sources_list,
                    load_result.rule_sources[static_cast<size_t>(group)]);

  base::Value* known_sources_list =
      rule_group_value.FindListKey(kKnownSourcesKey);
  if (known_sources_list)
    LoadKnownSources(*known_sources_list,
                     load_result.known_sources[static_cast<size_t>(group)]);

  base::Value* deleted_presets_list =
      rule_group_value.FindListKey(kDeletedPresetsKey);
  if (deleted_presets_list)
    LoadStringSetFromList(
        *deleted_presets_list,
        load_result.deleted_presets[static_cast<size_t>(group)]);

  base::Value* blocked_counters = rule_group_value.FindDictKey(kCountersKey);
  if (blocked_counters)
    LoadCounters(*blocked_counters,
                 load_result.blocked_counters[static_cast<size_t>(group)]);
}

void DoLoad(const base::FilePath& path,
            RuleServiceStorage::LoadingDoneCallback done_callback) {
  std::unique_ptr<RuleServiceStorage::LoadResult> load_result =
      std::make_unique<RuleServiceStorage::LoadResult>();

  JSONFileValueDeserializer serializer(path);
  std::unique_ptr<base::Value> root(serializer.Deserialize(nullptr, nullptr));

  if (root.get() && root->is_dict()) {
    base::Value* tracking_rules = root->FindDictKey(kTrackingRulesKey);
    if (tracking_rules) {
      LoadRulesGroup(RuleGroup::kTrackingRules, *tracking_rules, *load_result);
    }
    base::Value* ad_blocking_rules = root->FindDictKey(kAdBlockingRulesKey);
    if (ad_blocking_rules) {
      LoadRulesGroup(RuleGroup::kAdBlockingRules, *ad_blocking_rules,
                     *load_result);
    }
    absl::optional<int> version = root->FindIntKey(kVersionKey);
    if (version)
      load_result->storage_version =
          std::max(0, std::min(kCurrentStorageVersion, version.value()));
  }

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(std::move(done_callback), std::move(load_result)));
}

base::Value SerializeCounters(const std::map<std::string, int>& counters) {
  std::vector<std::pair<std::string, base::Value>> buffer;
  for (const auto& counter : counters) {
    buffer.emplace_back(counter.first, counter.second);
  }
  return base::Value(base::Value::DictStorage(std::move(buffer)));
}

base::Value SerializeSourcesList(
    const std::map<uint32_t, RuleSource>& rule_sources) {
  base::Value sources_list(base::Value::Type::LIST);
  for (const auto& map_entry : rule_sources) {
    const RuleSource& rule_source = map_entry.second;
    base::DictionaryValue source_value;
    if (rule_source.is_from_url)
      source_value.SetStringKey(kSourceUrlKey, rule_source.source_url.spec());
    else
      source_value.SetStringKey(kSourceFileKey,
                                rule_source.source_file.AsUTF8Unsafe());
    source_value.SetIntKey(kGroupKey, static_cast<int>(rule_source.group));
    source_value.SetBoolKey(kAllowAbpSnippets, rule_source.allow_abp_snippets);
    source_value.SetStringKey(kRulesListChecksumKey,
                              rule_source.rules_list_checksum);
    source_value.SetKey(kLastUpdateKey,
                        base::TimeToValue(rule_source.last_update));
    source_value.SetKey(kNextFetchKey,
                        base::TimeToValue(rule_source.next_fetch));
    source_value.SetIntKey(kValidRulesCountKey,
                           rule_source.rules_info.valid_rules);
    source_value.SetIntKey(kUnsupportedRulesCountKey,
                           rule_source.rules_info.unsupported_rules);
    source_value.SetIntKey(kInvalidRulesCountKey,
                           rule_source.rules_info.invalid_rules);
    source_value.SetIntKey(kLastFetchResultKey,
                           static_cast<int>(rule_source.last_fetch_result));
    source_value.SetBoolKey(kHasTrackerInfosKey, rule_source.has_tracker_infos);
    source_value.SetStringKey(kTitleKey,
                              rule_source.unsafe_adblock_metadata.title);
    source_value.SetStringKey(
        kHomePageKey, rule_source.unsafe_adblock_metadata.homepage.spec());
    source_value.SetStringKey(
        kLicenseKey, rule_source.unsafe_adblock_metadata.license.spec());
    source_value.SetStringKey(
        kRedirectKey, rule_source.unsafe_adblock_metadata.redirect.spec());
    source_value.SetKey(
        kVersionKey,
        base::Int64ToValue(rule_source.unsafe_adblock_metadata.version));
    source_value.SetKey(
        kExpiresKey,
        base::TimeDeltaToValue(rule_source.unsafe_adblock_metadata.expires));
    sources_list.Append(std::move(source_value));
  }

  return sources_list;
}

base::Value SerializeStringSetToList(const std::set<std::string>& string_set) {
  base::Value list(base::Value::Type::LIST);
  for (const auto& item : string_set) {
    list.Append(item);
  }

  return list;
}

base::Value SerializeKnownSourcesList(const KnownRuleSources& rule_sources) {
  base::Value sources_list(base::Value::Type::LIST);
  for (const auto& map_entry : rule_sources) {
    const KnownRuleSource& rule_source = map_entry.second;
    if (!rule_source.removable)
      continue;

    base::DictionaryValue source_value;
    if (rule_source.is_from_url)
      source_value.SetStringKey(kSourceUrlKey, rule_source.source_url.spec());
    else
      source_value.SetStringKey(kSourceFileKey,
                                rule_source.source_file.AsUTF8Unsafe());
    source_value.SetIntKey(kGroupKey, static_cast<int>(rule_source.group));
    source_value.SetBoolKey(kAllowAbpSnippets, rule_source.allow_abp_snippets);
    if (!rule_source.preset_id.empty())
      source_value.SetStringKey(kPresetIdKey, rule_source.preset_id);
    sources_list.Append(std::move(source_value));
  }

  return sources_list;
}

base::Value SerializeRuleGroup(RuleService* service, RuleGroup group) {
  base::DictionaryValue rule_group;
  rule_group.SetIntKey(kExceptionsTypeKey,
                       service->GetActiveExceptionList(group));
  rule_group.SetKey(kProcessListKey,
                    SerializeStringSetToList(service->GetExceptions(
                        group, RuleService::kProcessList)));
  rule_group.SetKey(kExemptListKey,
                    SerializeStringSetToList(service->GetExceptions(
                        group, RuleService::kExemptList)));
  rule_group.SetBoolKey(kEnabledKey, service->IsRuleGroupEnabled(group));

  rule_group.SetKey(kRuleSourcesKey,
                    SerializeSourcesList(service->GetRuleSources(group)));
  rule_group.SetKey(kKnownSourcesKey,
                    SerializeKnownSourcesList(
                        service->GetKnownSourcesHandler()->GetSources(group)));
  rule_group.SetKey(
      kDeletedPresetsKey,
      SerializeStringSetToList(
          service->GetKnownSourcesHandler()->GetDeletedPresets(group)));
  rule_group.SetStringKey(kIndexChecksum,
                          service->GetRulesIndexChecksum(group));
  rule_group.SetKey(
      kCountersKey,
      SerializeCounters(service->GetBlockerUrlsReporter()
                            ->GetBlockedDomains()[static_cast<size_t>(group)]));

  return std::move(rule_group);
}

}  // namespace

RuleServiceStorage::LoadResult::LoadResult() = default;
RuleServiceStorage::LoadResult::~LoadResult() = default;

RuleServiceStorage::RuleServiceStorage(
    content::BrowserContext* context,
    RuleService* rule_service,
    base::SequencedTaskRunner* file_io_task_runner)
    : file_io_task_runner_(file_io_task_runner),
      rule_service_(rule_service),
      writer_(context->GetPath().Append(kSourcesFileName),
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
  file_io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DoLoad, writer_.path(),
                     base::BindOnce(&RuleServiceStorage::OnLoadFinished,
                                    weak_factory_.GetWeakPtr())));
}

void RuleServiceStorage::OnLoadFinished(
    std::unique_ptr<LoadResult> load_result) {
  std::move(loading_done_callback_).Run(std::move(load_result));
}

void RuleServiceStorage::ScheduleSave() {
  writer_.ScheduleWrite(this);
}

void RuleServiceStorage::OnRuleServiceShutdown() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

bool RuleServiceStorage::SerializeData(std::string* output) {
  base::DictionaryValue root;

  root.SetKey(kTrackingRulesKey,
              SerializeRuleGroup(rule_service_, RuleGroup::kTrackingRules));
  root.SetKey(kAdBlockingRulesKey,
              SerializeRuleGroup(rule_service_, RuleGroup::kAdBlockingRules));
  root.SetIntKey(kVersionKey, kCurrentStorageVersion);

  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  return serializer.Serialize(root);
}

}  // namespace adblock_filter

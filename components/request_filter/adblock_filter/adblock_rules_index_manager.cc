// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rules_index_manager.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/ad_blocker/utils.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/adblock_rules_index_builder.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_index_generated.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

namespace adblock_filter {

namespace {

const base::FilePath::StringType kIndexFileName = FILE_PATH_LITERAL("Index");

constexpr int kMaxIndexReadFailAllowed = 10;

enum class BufferType {
  kRulesList = 0,
  kIndex,
};

std::string GetRuleBufferFromFile(const base::FilePath& buffer_path,
                                  BufferType buffer_type,
                                  const std::string& checksum) {
  std::string buffer_contents;
  base::ReadFileToString(buffer_path, &buffer_contents);

  std::string version_header;
  switch (buffer_type) {
    case BufferType::kRulesList:
      version_header = GetRulesListVersionHeader();
      break;
    case BufferType::kIndex:
      version_header = GetIndexVersionHeader();
      break;
  }

  if (!base::StartsWith(buffer_contents, version_header,
                        base::CompareCase::SENSITIVE)) {
    return std::string();
  }

  // Strip the header from |buffer_contents|.
  buffer_contents.erase(0, version_header.size());

  auto buffer =
      base::make_span(reinterpret_cast<const uint8_t*>(buffer_contents.data()),
                      buffer_contents.size());

  // Copy of the default values taken by the flatbuffers::Verifier constructor
  constexpr int kFlatBufferVerifierDefaultMaxDepth = 64;
  constexpr int kFlatBufferVerifierDefaultMaxTables = 1000000;

  // Large indexes can go a bit above the default limit. Rising the limit by two
  // orders of magnitude for those should be safe
  constexpr int kMaxTablesForIndex = kFlatBufferVerifierDefaultMaxTables * 100;

  flatbuffers::Verifier verifier(
      buffer.data(), buffer.size(), kFlatBufferVerifierDefaultMaxDepth,
      buffer_type == BufferType::kIndex ? kMaxTablesForIndex
                                        : kFlatBufferVerifierDefaultMaxTables);
  if (checksum != CalculateBufferChecksum(buffer))
    return std::string();

  switch (buffer_type) {
    case BufferType::kRulesList:
      return flat::VerifyRulesListBuffer(verifier) ? buffer_contents
                                                   : std::string();
    case BufferType::kIndex:
      return flat::VerifyRulesIndexBuffer(verifier) ? buffer_contents
                                                    : std::string();
  }
}
}  // namespace

RuleBufferHolder::RuleBufferHolder(std::string rule_buffer,
                                   const std::string& checksum)
    : rule_buffer_(std::move(rule_buffer)),
      checksum_(checksum),
      rules_list_(flat::GetRulesList(rule_buffer_.data())) {}

RuleBufferHolder::~RuleBufferHolder() = default;

RulesIndexManager::RulesIndexManager(
    content::BrowserContext* context,
    RuleService* rule_service,
    RuleGroup group,
    const std::string& index_checksum,
    RulesIndexChangedCallback rules_index_change_callback,
    RulesIndexLoadedCallback rules_index_loaded_callback,
    RulesBufferReadFailCallback rule_buffer_read_fail_callback,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : group_(group),
      reload_in_progress_(!index_checksum.empty()),
      rule_sources_(rule_service->GetRuleManager()->GetRuleSources(group)),
      rules_list_folder_(context->GetPath()
                             .Append(GetRulesFolderName())
                             .Append(GetGroupFolderName(group_))),
      rules_index_change_callback_(rules_index_change_callback),
      rules_index_loaded_callback_(rules_index_loaded_callback),
      rule_buffer_read_fail_callback_(rule_buffer_read_fail_callback),
      file_task_runner_(file_task_runner),
      weak_factory_(this) {
  rule_service->GetRuleManager()->AddObserver(this);

  for (const auto& [rule_source_id, rule_source] : rule_sources_) {
    if (!rule_source.rules_list_checksum.empty()) {
      ReadRules(rule_source);
    }
  }

  if (index_checksum.empty()) {
    // We don't have an index yet, or the last attempt to rebuild it failed.
    // Do one rebuild attempt now.
    RebuildIndex();
  } else {
    // Since we are using a sequenced task runner, by scheduling this read last,
    // it completes after all the sources read. By the same reasoning,
    // OnIndexRead will also be called after the last OnRulesRead call.
    ReadIndex(index_checksum);
  }
}

RulesIndexManager::~RulesIndexManager() = default;

void RulesIndexManager::OnRuleSourceUpdated(
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
      ReadRules(rule_source);
    // Make sure to drop the |old_source| iterator here sisnce we're about to
    // change the map.
  }

  rule_sources_.insert_or_assign(rule_source.core.id(), rule_source);
}

void RulesIndexManager::OnRuleSourceDeleted(uint32_t source_id,
                                            RuleGroup group) {
  if (group != group_)
    return;

  // Keep any rules buffer around for the index currently in use, they'll be
  // cleared once the new index is ready.
  if (rules_buffers_.count(source_id) != 0) {
    old_rules_buffers_.push_back(std::move(rules_buffers_[source_id]));
  }

  rule_sources_.erase(source_id);
  rules_buffers_.erase(source_id);

  RebuildIndex();
}

void RulesIndexManager::OnExceptionListStateChanged(RuleGroup group) {
  if (group != group_)
    return;

  if (rules_index_)
    rules_index_->InvalidateActivationCache();
}

void RulesIndexManager::OnExceptionListChanged(
    RuleGroup group,
    RuleManager::ExceptionsList list) {
  if (group != group_)
    return;

  if (rules_index_)
    rules_index_->InvalidateActivationCache();
}

void RulesIndexManager::ReadRules(const ActiveRuleSource& rule_source) {
  if (rule_source.last_fetch_result == FetchResult::kFileUnsupported) {
    // We know there is no valid rules here. No point in trying.
    // Keep any rules buffer around for the index currently in use, they'll be
    // cleared once the new index is ready.
    if (rules_buffers_.count(rule_source.core.id()) != 0) {
      old_rules_buffers_.push_back(
          std::move(rules_buffers_[rule_source.core.id()]));
      rules_buffers_.erase(rule_source.core.id());
      RebuildIndex();
    }
    return;
  }

  CHECK(!rule_source.rules_list_checksum.empty());

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetRuleBufferFromFile,
                     rules_list_folder_.AppendASCII(
                         base::NumberToString(rule_source.core.id())),
                     BufferType::kRulesList, rule_source.rules_list_checksum),
      base::BindOnce(&RulesIndexManager::OnRulesRead,
                     weak_factory_.GetWeakPtr(), rule_source.core.id(),
                     rule_source.rules_list_checksum));
}

void RulesIndexManager::OnRulesRead(uint32_t source_id,
                                    const std::string& checksum,
                                    std::string rules_buffer) {
  const auto& rule_source = rule_sources_.find(source_id);

  if (rule_source == rule_sources_.end()) {
    // The rule source was removed while we were fetching its buffer.
    return;
  }

  if (rule_source->second.rules_list_checksum != checksum) {
    // The rule source was modified while we were fetching its buffer.
    return;
  }

  if (rules_buffer.empty()) {
    // If we had a rules buffer for this source already, keep it for now.
    rule_buffer_read_fail_callback_.Run(group_, source_id);
    return;
  }

  // Keep any rules buffer around for the index currently in use, they'll be
  // cleared once the new index is ready.
  if (rules_buffers_.count(source_id) != 0) {
    old_rules_buffers_.push_back(std::move(rules_buffers_[source_id]));
  }

  rules_buffers_[source_id] =
      std::make_unique<RuleBufferHolder>(std::move(rules_buffer), checksum);

  RebuildIndex();
}

void RulesIndexManager::OnIndexRead(std::string index_buffer) {
  reload_in_progress_ = false;

  std::unique_ptr<RulesIndex> new_index;
  bool uses_all_buffers = false;
  if (!index_buffer.empty()) {
    RulesIndex::RulesBufferMap index_rules_buffer;
    for (const auto& buffer : rules_buffers_) {
      index_rules_buffer.insert({buffer.first, *buffer.second});
    }
    new_index = RulesIndex::CreateInstance(
        index_rules_buffer, std::move(index_buffer), &uses_all_buffers);
  }

  if (!new_index) {
    // Some sources changed while rebuilding or reloading or the index was
    // corrupted. Rebuild it.
    if (++index_read_fail_count_ < kMaxIndexReadFailAllowed)
      RebuildIndex();
    return;
  }

  index_read_fail_count_ = 0;

  rules_index_.swap(new_index);
  old_rules_buffers_.clear();

  rules_index_loaded_callback_.Run();

  if (!uses_all_buffers) {
    // The index we loaded doesn't reference all our rule buffers. This
    // means several reads have completed since the last rebuild.
    // We should build a new index while we use the one we just set up.
    RebuildIndex();
  }
}

void RulesIndexManager::RebuildIndex() {
  if (reload_in_progress_) {
    // A reload is already in progress. Unless this is the initial loading, it
    // is likely going to fail initializing the new index because the source
    // checksums aren't matching anymore. We could save some time and not wait
    // for that failure, at the expense of more complex code.
    return;
  }

  BuildAndSaveIndex(rules_buffers_, file_task_runner_.get(),
                    rules_list_folder_.Append(kIndexFileName),
                    base::BindOnce(&RulesIndexManager::ReadIndex,
                                   weak_factory_.GetWeakPtr()));
  reload_in_progress_ = true;
}

void RulesIndexManager::ReadIndex(const std::string& checksum) {
  index_checksum_ = checksum;
  rules_index_change_callback_.Run();

  if (checksum.empty()) {
    // Saving failed. This can only happen if writing to the file failed, so
    // it's unlikely that just retrying will solve the issue, so we just abort
    // here.
    reload_in_progress_ = false;
    return;
  }
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetRuleBufferFromFile,
                     rules_list_folder_.Append(kIndexFileName),
                     BufferType::kIndex, index_checksum_),
      base::BindOnce(&RulesIndexManager::OnIndexRead,
                     weak_factory_.GetWeakPtr()));
}

}  // namespace adblock_filter

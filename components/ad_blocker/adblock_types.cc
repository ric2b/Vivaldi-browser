// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_types.h"

#include "base/hash/hash.h"

namespace adblock_filter {
AdBlockMetadata::AdBlockMetadata() = default;
AdBlockMetadata::~AdBlockMetadata() = default;
AdBlockMetadata::AdBlockMetadata(const AdBlockMetadata&) = default;

bool AdBlockMetadata::operator==(const AdBlockMetadata& other) const {
  return homepage == other.homepage && title == other.title &&
         expires == other.expires && license == other.license &&
         redirect == other.redirect && version == other.version;
}

/*static*/
std::optional<RuleSourceCore> RuleSourceCore::FromUrl(GURL url) {
  if (!url.is_valid() || url.is_empty())
    return std::nullopt;

  return RuleSourceCore(std::move(url));
}

/*static*/
std::optional<RuleSourceCore> RuleSourceCore::FromFile(base::FilePath file) {
  if (file.empty() || !file.IsAbsolute() || file.ReferencesParent() ||
      file.EndsWithSeparator())
    return std::nullopt;

  return RuleSourceCore(std::move(file));
}

RuleSourceCore::RuleSourceCore(GURL source_url)
    : source_url_(std::move(source_url)),
      id_(base::PersistentHash(source_url_->spec())) {}
RuleSourceCore::RuleSourceCore(base::FilePath source_file)
    : source_file_(std::move(source_file)),
      id_(base::PersistentHash(source_file_->AsUTF8Unsafe())) {}
RuleSourceCore::~RuleSourceCore() = default;
RuleSourceCore::RuleSourceCore(const RuleSourceCore&) = default;
RuleSourceCore& RuleSourceCore::operator=(const RuleSourceCore&) = default;
RuleSourceCore::RuleSourceCore(RuleSourceCore&&) = default;
RuleSourceCore& RuleSourceCore::operator=(RuleSourceCore&&) = default;

KnownRuleSource::KnownRuleSource(RuleSourceCore core) : core(std::move(core)) {}
KnownRuleSource::~KnownRuleSource() = default;
KnownRuleSource::KnownRuleSource(const KnownRuleSource&) = default;
KnownRuleSource& KnownRuleSource::operator=(const KnownRuleSource&) = default;
KnownRuleSource::KnownRuleSource(KnownRuleSource&&) = default;
KnownRuleSource& KnownRuleSource::operator=(KnownRuleSource&&) = default;

ActiveRuleSource::ActiveRuleSource(RuleSourceCore core)
    : core(std::move(core)) {}
ActiveRuleSource::~ActiveRuleSource() = default;
ActiveRuleSource::ActiveRuleSource(const ActiveRuleSource&) = default;
ActiveRuleSource& ActiveRuleSource::operator=(const ActiveRuleSource&) =
    default;

}  // namespace adblock_filter

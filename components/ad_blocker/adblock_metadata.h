// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_METADATA_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_METADATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace adblock_filter {

struct AdBlockMetadata {
  AdBlockMetadata();
  ~AdBlockMetadata();
  AdBlockMetadata(const AdBlockMetadata&);

  bool operator==(const AdBlockMetadata& other) const;

  GURL homepage;
  std::string title;
  base::TimeDelta expires;
  GURL license;
  GURL redirect;
  int64_t version = 0;
};

struct RulesInfo {
  int valid_rules = 0;
  int unsupported_rules = 0;
  int invalid_rules = 0;
};

enum class RuleGroup {
  kTrackingRules = 0,
  kAdBlockingRules,

  kFirst = kTrackingRules,
  kLast = kAdBlockingRules,
};
constexpr size_t kRuleGroupCount = static_cast<size_t>(RuleGroup::kLast) + 1;

enum class FetchResult {
  kSuccess = 0,
  kDownloadFailed,
  kFileNotFound,
  kFileReadError,
  kFileUnsupported,
  kFailedSavingParsedRules,
  kUnknown,

  kFirst = kSuccess,
  kLast = kUnknown
};

struct RuleSourceBase {
  RuleSourceBase(const GURL& source_url, RuleGroup group);
  RuleSourceBase(const base::FilePath& source_file, RuleGroup group);
  virtual ~RuleSourceBase();
  RuleSourceBase(const RuleSourceBase&);
  RuleSourceBase& operator=(const RuleSourceBase&);

  GURL source_url;
  base::FilePath source_file;
  bool allow_abp_snippets = false;
  bool is_from_url;
  RuleGroup group;
  uint32_t id;

 protected:
  RuleSourceBase();
};

// Known rule-sources can be manipulated by the user.
struct KnownRuleSource : public RuleSourceBase {
  KnownRuleSource(const GURL& source_url, RuleGroup group);
  KnownRuleSource(const base::FilePath& source_file, RuleGroup group);
  ~KnownRuleSource() override;
  KnownRuleSource(const KnownRuleSource&);
  KnownRuleSource& operator=(const KnownRuleSource&);

  bool removable = true;
  std::string preset_id = "";
};

using KnownRuleSources = std::map<uint32_t, KnownRuleSource>;

// Rule-sources are the rule-sources that are currently actively in use
// by the adblock engine.
struct RuleSource : public RuleSourceBase {
  explicit RuleSource(const KnownRuleSource& known_source);
  RuleSource(const GURL& source_url, RuleGroup group);
  RuleSource(const base::FilePath& source_file, RuleGroup group);
  ~RuleSource() override;
  RuleSource(const RuleSource&);
  RuleSource& operator=(const RuleSource&);

  std::string rules_list_checksum;
  // These are pulled directly from the rules file with minimal validation.
  AdBlockMetadata unsafe_adblock_metadata;
  base::Time last_update;
  base::Time next_fetch;
  bool is_fetching = false;
  FetchResult last_fetch_result = FetchResult::kUnknown;
  RulesInfo rules_info;
  bool has_tracker_infos = false;
};

// Usually, we'll want to manipulate lists of rule sources.
using RuleSources = std::vector<RuleSource>;
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_METADATA_H_

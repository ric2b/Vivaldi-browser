// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_TYPES_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_TYPES_H_

#include <map>
#include <optional>
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

struct RuleSourceSettings {
  bool allow_abp_snippets = false;
  bool naked_hostname_is_pure_host = true;
  bool use_whole_document_allow = true;
  bool allow_attribution_tracker_rules = false;
};

class RuleSourceCore {
 public:
  static std::optional<RuleSourceCore> FromUrl(GURL url);
  static std::optional<RuleSourceCore> FromFile(base::FilePath file);

  virtual ~RuleSourceCore();
  RuleSourceCore(const RuleSourceCore&);
  RuleSourceCore& operator=(const RuleSourceCore&);
  RuleSourceCore(RuleSourceCore&&);
  RuleSourceCore& operator=(RuleSourceCore&&);

  GURL source_url() const { return *source_url_; }
  base::FilePath source_file() const { return *source_file_; }
  bool is_from_url() const { return source_url_.has_value(); }
  const RuleSourceSettings& settings() const { return settings_; }
  void set_settings(RuleSourceSettings settings) { settings_ = settings; }
  uint32_t id() const { return id_; }

 private:
  explicit RuleSourceCore(GURL source_url);
  explicit RuleSourceCore(base::FilePath source_file);
  std::optional<GURL> source_url_;
  std::optional<base::FilePath> source_file_;
  RuleSourceSettings settings_;
  uint32_t id_;
};

struct KnownRuleSource {
  explicit KnownRuleSource(RuleSourceCore core);
  ~KnownRuleSource();
  KnownRuleSource(const KnownRuleSource&);
  KnownRuleSource& operator=(const KnownRuleSource&);
  KnownRuleSource(KnownRuleSource&&);
  KnownRuleSource& operator=(KnownRuleSource&&);

  RuleSourceCore core;
  bool removable = true;
  std::string preset_id = "";
};

using KnownRuleSources = std::map<uint32_t, KnownRuleSource>;

// Rule-sources are the rule-sources that are currently actively in use
// by the adblock engine.
struct ActiveRuleSource {
  explicit ActiveRuleSource(RuleSourceCore core);
  ~ActiveRuleSource();
  ActiveRuleSource(const ActiveRuleSource&);
  ActiveRuleSource& operator=(const ActiveRuleSource&);

  RuleSourceCore core;
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
using ActiveRuleSources = std::vector<ActiveRuleSource>;
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_TYPES_H_

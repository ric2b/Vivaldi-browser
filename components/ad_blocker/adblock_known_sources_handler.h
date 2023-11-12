// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_KNOWN_SOURCES_HANDLER_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_KNOWN_SOURCES_HANDLER_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/ad_blocker/adblock_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace adblock_filter {
/* This class is designed to help the UI with keeping track of well-known rule
sources that may or may not be in use by the adblock RuleService. It can be
used as an alternative to adding and removing rule source directly from the
adblock RuleService. It also holds the address of predefined rule sources.*/
class KnownRuleSourcesHandler {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    virtual void OnKnownSourceAdded(const KnownRuleSource& rule_source) {}
    virtual void OnKnownSourceRemoved(RuleGroup group, uint32_t source_id) {}

    virtual void OnKnownSourceEnabled(RuleGroup group, uint32_t source_id) {}
    virtual void OnKnownSourceDisabled(RuleGroup group, uint32_t source_id) {}
  };

  virtual ~KnownRuleSourcesHandler();

  virtual const KnownRuleSources& GetSources(RuleGroup group) const = 0;
  virtual const std::set<std::string>& GetDeletedPresets(
      RuleGroup group) const = 0;

  virtual absl::optional<uint32_t> AddSourceFromUrl(RuleGroup group,
                                                    const GURL& url) = 0;
  virtual absl::optional<uint32_t> AddSourceFromFile(
      RuleGroup group,
      const base::FilePath& file) = 0;
  virtual absl::optional<KnownRuleSource> GetSource(RuleGroup group,
                                                    uint32_t source_id) = 0;
  virtual bool RemoveSource(RuleGroup group, uint32_t source_id) = 0;

  virtual bool EnableSource(RuleGroup group, uint32_t source_id) = 0;
  virtual void DisableSource(RuleGroup group, uint32_t source_id) = 0;
  virtual bool IsSourceEnabled(RuleGroup group, uint32_t source_id) = 0;

  virtual void ResetPresetSources(RuleGroup group) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_KNOWN_SOURCES_HANDLER_H_

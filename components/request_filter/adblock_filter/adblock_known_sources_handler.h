// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_KNOWN_SOURCES_HANDLER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_KNOWN_SOURCES_HANDLER_H_

#include <array>
#include <map>

#include "base/callback.h"
#include "base/observer_list.h"
#include "components/request_filter/adblock_filter/adblock_metadata.h"
#include "url/gurl.h"

namespace adblock_filter {
class RuleServiceImpl;

struct KnownRuleSource : public RuleSourceBase {
  KnownRuleSource(const GURL& source_url, RuleGroup group);
  KnownRuleSource(const base::FilePath& source_file, RuleGroup group);
  ~KnownRuleSource() override;
  KnownRuleSource(const KnownRuleSource&);

  bool removable = true;
  std::string preset_id = "";
};

using KnownRuleSources = std::map<uint32_t, KnownRuleSource>;

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

  KnownRuleSourcesHandler(
      RuleServiceImpl* rule_service,
      int storage_version,
      const std::array<std::vector<KnownRuleSource>, kRuleGroupCount>&
          known_sources,
      std::array<std::set<std::string>, kRuleGroupCount> deleted_presets,
      base::RepeatingClosure schedule_save);
  ~KnownRuleSourcesHandler();

  const KnownRuleSources& GetSources(RuleGroup group) const;
  const std::set<std::string>& GetDeletedPresets(RuleGroup group) const;

  base::Optional<uint32_t> AddSourceFromUrl(RuleGroup group, const GURL& url);
  base::Optional<uint32_t> AddSourceFromFile(RuleGroup group,
                                             const base::FilePath& file);
  base::Optional<KnownRuleSource> GetSource(RuleGroup group,
                                            uint32_t source_id);
  bool RemoveSource(RuleGroup group, uint32_t source_id);

  bool EnableSource(RuleGroup group, uint32_t source_id);
  void DisableSource(RuleGroup group, uint32_t source_id);
  bool IsSourceEnabled(RuleGroup group, uint32_t source_id);

  void ResetPresetSources(RuleGroup group);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::Optional<uint32_t> AddSource(const KnownRuleSource& known_source,
                                     bool enable);

  KnownRuleSources& GetSourceMap(RuleGroup group);
  const KnownRuleSources& GetSourceMap(RuleGroup group) const;

  void UpdateSourcesFromPresets(RuleGroup group,
                                bool add_deleted_presets,
                                bool store_missing_as_deleted);

  RuleServiceImpl* rule_service_;

  std::array<KnownRuleSources, kRuleGroupCount> known_sources_;
  std::array<std::set<std::string>, kRuleGroupCount> deleted_presets_;

  base::ObserverList<Observer> observers_;

  base::RepeatingClosure schedule_save_;

  DISALLOW_COPY_AND_ASSIGN(KnownRuleSourcesHandler);
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_KNOWN_SOURCES_HANDLER_H_

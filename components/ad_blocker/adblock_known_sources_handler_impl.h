// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_KNOWN_SOURCES_HANDLER_IMPL_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_KNOWN_SOURCES_HANDLER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/ad_blocker/adblock_known_sources_handler.h"
#include "components/ad_blocker/adblock_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace adblock_filter {
class RuleService;

class KnownRuleSourcesHandlerImpl : public KnownRuleSourcesHandler {
 public:
  KnownRuleSourcesHandlerImpl(
      RuleService* rule_service,
      int storage_version,
      const std::string& locale,
      const std::array<std::vector<KnownRuleSource>, kRuleGroupCount>&
          known_sources,
      std::array<std::set<std::string>, kRuleGroupCount> deleted_presets,
      base::RepeatingClosure schedule_save);
  ~KnownRuleSourcesHandlerImpl() override;
  KnownRuleSourcesHandlerImpl(const KnownRuleSourcesHandlerImpl&) = delete;
  KnownRuleSourcesHandlerImpl& operator=(const KnownRuleSourcesHandlerImpl&) =
      delete;

  const KnownRuleSources& GetSources(RuleGroup group) const override;
  const std::set<std::string>& GetDeletedPresets(
      RuleGroup group) const override;

  bool AddSource(RuleGroup group, RuleSourceCore source_core) override;
  std::optional<KnownRuleSource> GetSource(RuleGroup group,
                                           uint32_t source_id) override;
  bool RemoveSource(RuleGroup group, uint32_t source_id) override;

  bool EnableSource(RuleGroup group, uint32_t source_id) override;
  void DisableSource(RuleGroup group, uint32_t source_id) override;
  bool IsSourceEnabled(RuleGroup group, uint32_t source_id) override;

  bool SetSourceSettings(RuleGroup group,
                         uint32_t source_id,
                         RuleSourceSettings settings) override;

  void ResetPresetSources(RuleGroup group) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  bool AddSource(RuleGroup group, KnownRuleSource known_source, bool enable);

  KnownRuleSources& GetSourceMap(RuleGroup group);
  const KnownRuleSources& GetSourceMap(RuleGroup group) const;

  void UpdateSourcesFromPresets(RuleGroup group,
                                bool add_deleted_presets,
                                bool store_missing_as_deleted);

  const raw_ptr<RuleService> rule_service_;

  std::array<KnownRuleSources, kRuleGroupCount> known_sources_;
  std::array<std::set<std::string>, kRuleGroupCount> deleted_presets_;

  base::ObserverList<Observer> observers_;

  base::RepeatingClosure schedule_save_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_KNOWN_SOURCES_HANDLER_IMPL_H_

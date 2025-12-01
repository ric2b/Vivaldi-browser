// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_DOCUMENT_ACTIVATIONS_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_DOCUMENT_ACTIVATIONS_H_

#include <array>

#include "components/ad_blocker/content/adblock_rules_index.h"
#include "content/public/browser/document_user_data.h"

namespace adblock_filter {
class StateAndLogsImpl;

class DocumentActivations
    : public content::DocumentUserData<DocumentActivations> {
 public:
  static RulesIndex::ActivationResults GetActivations(
      RuleGroup group,
      const content::RenderFrameHost* rfh);

  static bool IsAdAttributionArmed(const content::RenderFrameHost* rfh);

  ~DocumentActivations();

 private:
  DocumentActivations(
      content::RenderFrameHost* rfh,
      std::array<RulesIndex::ActivationResults, kRuleGroupCount> activations);

  std::array<RulesIndex::ActivationResults, kRuleGroupCount> activations_;

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};
}  // namespace adblock_filter
#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_DOCUMENT_ACTIVATIONS_H_

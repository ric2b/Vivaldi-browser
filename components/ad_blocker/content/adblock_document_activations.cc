// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_document_activations.h"

namespace adblock_filter {

DOCUMENT_USER_DATA_KEY_IMPL(DocumentActivations);

DocumentActivations::DocumentActivations(
    content::RenderFrameHost* rfh,
    std::array<RulesIndex::ActivationResults, kRuleGroupCount> activations)
    : content::DocumentUserData<DocumentActivations>(rfh),
      activations_(std::move(activations)) {}

DocumentActivations::~DocumentActivations() = default;

/* static */
bool DocumentActivations::IsAdAttributionArmed(
    const content::RenderFrameHost* rfh) {
  const DocumentActivations* activations = GetForCurrentDocument(rfh);
  if (!activations) {
    return false;
  }
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    const auto& activations_by_type =
        activations->activations_[static_cast<size_t>(group)].by_type;
    if (activations_by_type.contains(flat::ActivationType_ATTRIBUTE_ADS) &&
        activations_by_type.at(flat::ActivationType_ATTRIBUTE_ADS)
            .IsDecision(flat::Decision_PASS)) {
      return true;
    }
  }

  return false;
}

/* static */
RulesIndex::ActivationResults DocumentActivations::GetActivations(
    RuleGroup group,
    const content::RenderFrameHost* rfh) {
  if (!rfh) {
    // Convenience. If the calling code doesn't actually have a frame to work
    // with, it'll want the default activations
    return RulesIndex::ActivationResults{};
  }

  const DocumentActivations* activations = GetForCurrentDocument(rfh);
  if (!activations) {
    // This can happen for documents which are not part of a tab and for which
    // no request resulted in trying to filter the URL. Assume they just have
    // default activations.
    return RulesIndex::ActivationResults{};
  }

  return activations->activations_[static_cast<size_t>(group)];
}
}  // namespace adblock_filter
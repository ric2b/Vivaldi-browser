// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_DOCUMENT_ACTIVATIONS_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_DOCUMENT_ACTIVATIONS_H_

#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "content/public/browser/document_user_data.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}

namespace adblock_filter {
class StateAndLogsImpl;

class DocumentState : public content::DocumentUserData<DocumentState> {
 public:
  static void ApplyParentActivations(
      RuleGroup group,
      const content::RenderFrameHost* parent_frame,
      ActivationResults& local_activations);

  static const ActivationResults& GetActivations(
      RuleGroup group,
      const content::RenderFrameHost* rfh);

  ~DocumentState();

 private:
  DocumentState(content::RenderFrameHost* rfh,
                content::NavigationHandle* navigation);

  RuleGroupArray<ActivationResults> activations_;

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};
}  // namespace adblock_filter
#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_DOCUMENT_ACTIVATIONS_H_

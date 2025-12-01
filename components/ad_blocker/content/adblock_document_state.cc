// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_document_state.h"
#include "components/ad_blocker/content/adblock_navigation_tracker_impl.h"
#include "components/ad_blocker/content/utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

namespace adblock_filter {

DOCUMENT_USER_DATA_KEY_IMPL(DocumentState);

DocumentState::DocumentState(content::RenderFrameHost* rfh,
                             content::NavigationHandle* navigation)
    : content::DocumentUserData<DocumentState>(rfh) {
  // An uncommited navigation should not have a valid document to call this on,
  // and this should already exist when a bf-cache navigation takes place, so
  // the constructor shouldn't be called either.
  CHECK(navigation->HasCommitted() &&
        !navigation->IsServedFromBackForwardCache() &&
        navigation->GetRenderFrameHost() == &render_frame_host());

  if (navigation->IsErrorPage()) {
    // Error pages use the default activations
    return;
  }

  for (auto [group, activations] : activations_) {
    activations = NavigationTrackerImpl::GetForNavigationHandle(*navigation)
                      ->GetActivations(group);
  }
}

DocumentState::~DocumentState() = default;

/*static*/
void DocumentState::ApplyParentActivations(
    RuleGroup group,
    const content::RenderFrameHost* parent_frame,
    ActivationResults& local_activations) {
  if (!parent_frame) {
    return;
  }

  const DocumentState* parent_document_state =
      GetForCurrentDocument(parent_frame);

  if (!parent_document_state) {
    return;
  }

  const ActivationResults& parent_activations =
      parent_document_state->activations_[group];

  if (parent_activations.document_exception) {
    local_activations.document_exception = true;
  }

  for (const auto [type, parent_activation] : parent_activations.by_type) {
    ActivationResult& local_activation = local_activations.by_type[type];
    if ((local_activation.IsDecision(RuleDecision::kModifyImportant)))
      continue;

    if (!parent_activation.rule_stub) {
      continue;
    }

    if (!local_activation.rule_stub ||
        local_activation.rule_stub->priority <
            parent_activation.rule_stub->priority) {
      local_activation.rule_stub = parent_activation.rule_stub;
      local_activation.from_parent = true;
    }
  }

  return;
}

/* static */
const ActivationResults& DocumentState::GetActivations(
    RuleGroup group,
    const content::RenderFrameHost* rfh) {
  if (!rfh) {
    // Convenience. If the calling code doesn't actually have a frame to work
    // with, it'll want the default activations
    return kEmptyActivationResults;
  }

  const DocumentState* state = GetForCurrentDocument(rfh);
  if (!state) {
    // This can happen for documents for which no navigation occured, and no
    // request resulted in trying to filter the URL. Assume they just have
    // their parents activations or failing that, the default ones.
    DCHECK(!CanFilterUrl(rfh->GetLastCommittedURL(), false));
    return rfh->GetParent() ? GetActivations(group, rfh->GetParent())
                            : kEmptyActivationResults;
  }

  return state->activations_[group];
}
}  // namespace adblock_filter

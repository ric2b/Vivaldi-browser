// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_navigation_tracker_impl.h"

#include "components/ad_blocker/content/adblock_document_state.h"
#include "components/ad_blocker/content/adblock_state_and_logs_impl.h"
#include "content/public/browser/render_frame_host.h"

namespace adblock_filter {
NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationTrackerImpl);

NavigationTrackerImpl::NavigationTrackerImpl(
    content::NavigationHandle& navigation_handle,
    base::WeakPtr<StateAndLogsImpl> state_and_logs)
    : navigation_handle_(navigation_handle),
      navigation_id_(navigation_handle.GetNavigationId()),
      state_and_logs_(std::move(state_and_logs)) {
  state_and_logs_->OnNavigationTrackerCreated(this);
}

NavigationTrackerImpl::~NavigationTrackerImpl() {
  if (state_and_logs_) {
    state_and_logs_->OnNavigationTrackerDestroyed(this);
  }
}

void NavigationTrackerImpl::OnBlockedByRule(RuleGroup group,
                                            const RequestFilterRuleStub& stub) {
  // Only record the first source of blocking
  if (!blocked_by_rule_) {
    blocked_by_rule_ = {group, stub};
  }
}

std::optional<std::pair<RuleGroup, RequestFilterRuleStub>>
NavigationTrackerImpl::GetBlockedByRule() const {
  return blocked_by_rule_;
}

bool NavigationTrackerImpl::UpdateActivationsIfNeeded(RuleGroup group) {
  GURL url = navigation_handle_->GetURL();
  content::RenderFrameHost* parent_frame = navigation_handle_->GetParentFrame();

  if (determined_for_url_[group] == url) {
    // Our cache is current.
    return true;
  }

  // If we are shutting down, activations don't matter anymore as everything
  // will be blocked. We'll just skip populating them.
  if (!state_and_logs_) {
    return false;
  }

  url::Origin parent_origin = parent_frame
                                  ? parent_frame->GetLastCommittedOrigin()
                                  : url::Origin::Create(url);

  std::optional<ActivationResults> local_activations =
      state_and_logs_->GetLocalActivations(group, parent_origin, url);
  if (!local_activations) {
    // Indexes are not yet ready. This can happen when this is called as part of
    // handling navigations. In such case, we'd rather not record anything and
    // wait until it's called again as part of the actual request, which does
    // wait for the indexes to be ready.
    return false;
  }
  DocumentState::ApplyParentActivations(group, parent_frame,
                                        *local_activations);

  activations_[group] = std::move(*local_activations);
  determined_for_url_[group] = url;
  return true;
}

const ActivationResults& NavigationTrackerImpl::GetActivations(
    RuleGroup group) {
  if (!UpdateActivationsIfNeeded(group)) {
    // Either the index hasn't yet been loaded or the service has gone away.
    // Since activations operate as modifiers for other rules and no rules can
    // be retrieved at this point, it doesn't matter what we return. Just use
    // the default.
    return kEmptyActivationResults;
  }

  CHECK_EQ(navigation_handle_->GetURL(), determined_for_url_[group]);
  return activations_[group];
}
}  // namespace adblock_filter

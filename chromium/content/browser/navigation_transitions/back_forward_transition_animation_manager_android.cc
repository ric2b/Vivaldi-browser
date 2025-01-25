// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"

#include "content/browser/navigation_transitions/back_forward_transition_animator.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util_android.h"

namespace content {

namespace {

using NavigationDirection =
    BackForwardTransitionAnimationManager::NavigationDirection;

using AnimationStage = BackForwardTransitionAnimationManager::AnimationStage;
using SwipeEdge = ui::BackGestureEventSwipeEdge;

bool ShouldSkipDefaultNavTransitionForPendingUX(
    NavigationDirection nav_direction,
    SwipeEdge edge) {
  SwipeEdge back_edge = !l10n_util::ShouldMirrorBackForwardGestures()
                            ? SwipeEdge::LEFT
                            : SwipeEdge::RIGHT;

  // Currently we only have approved UX for the history back navigation on the
  // back edge (left in LTR), in both gesture mode and 3-button mode.
  if (nav_direction == NavigationDirection::kBackward && edge == back_edge) {
    return false;
  }

  return true;
}

}  // namespace

BackForwardTransitionAnimationManagerAndroid::
    BackForwardTransitionAnimationManagerAndroid(
        WebContentsViewAndroid* web_contents_view_android,
        NavigationControllerImpl* navigation_controller)
    : web_contents_view_android_(web_contents_view_android),
      navigation_controller_(navigation_controller),
      animator_factory_(
          std::make_unique<BackForwardTransitionAnimator::Factory>()) {}

BackForwardTransitionAnimationManagerAndroid::
    ~BackForwardTransitionAnimationManagerAndroid() = default;

void BackForwardTransitionAnimationManagerAndroid::OnGestureStarted(
    const ui::BackGestureEvent& gesture,
    SwipeEdge edge,
    NavigationDirection navigation_direction) {
  std::optional<int> index =
      navigation_direction == NavigationDirection::kForward
          ? navigation_controller_->GetIndexForGoForward()
          : navigation_controller_->GetIndexForGoBack();
  CHECK(index.has_value());
  auto* destination_entry = navigation_controller_->GetEntryAtIndex(*index);

  CHECK(destination_entry)
      << "The embedder should only delegate the history navigation task "
         "to this manager if there is a destination entry.";

  // Each previous gesture should finished with `OnGestureCancelled()` or
  // `OnGestureInvoked()`. In both cases we reset `destination_entry_index_` to
  // -1.
  CHECK_EQ(destination_entry_index_, -1);
  destination_entry_index_ = *index;

  if (animator_) {
    // It's possible for a user to start a second gesture when the first gesture
    // animation is still on-going (aka "chained back"). For now, abort the
    // previous animation (impl's dtor will reset the layer's position and
    // reclaim all the resources).
    //
    // TODO(crbug.com/40261105): We need a proper UX to support this.
    animator_.reset();
  }

  // Handle the case where the screenshot's dimension does not match the
  // physical viewport:
  // - TODO(https://crbug.com/340292683): Screenshot is captured with the URL
  // bar shown but used for transition where the URL bar is hidden (default
  // background color at the bottom).
  // - TODO(https://crbug.com/346979589): Screenshot is captured in a landscape
  // / portrait mode but used for transition in the different mode.
  if (ShouldSkipDefaultNavTransitionForPendingUX(navigation_direction, edge)) {
    return;
  }

  CHECK(animator_factory_);
  animator_ = animator_factory_->Create(
      web_contents_view_android_.get(), navigation_controller_.get(), gesture,
      navigation_direction, edge, destination_entry, this);

  // Become a WCO as soon as this class is created, because we want to
  // observe all navigations while this class is controlling the UI. This
  // allows us to ensure the visuals displayed align with the active page
  // and URL in the URL bar.
  WebContentsObserver::Observe(
      this->web_contents_view_android()->web_contents());
  auto* window = web_contents_view_android()->GetTopLevelNativeWindow();
  CHECK(window);
  window->AddObserver(this);
  web_contents_view_android()->GetNativeView()->AddObserver(this);

  OnAnimationStageChanged();
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureProgressed(
    const ui::BackGestureEvent& gesture) {
  if (animator_) {
    animator_->OnGestureProgressed(gesture);
  }
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureCancelled() {
  CHECK_NE(destination_entry_index_, -1);
  if (animator_) {
    animator_->OnGestureCancelled();
    MaybeDestroyAnimator();
  }
  destination_entry_index_ = -1;
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureInvoked() {
  CHECK_NE(destination_entry_index_, -1);
  if (animator_) {
    animator_->OnGestureInvoked();
    MaybeDestroyAnimator();
  } else {
    navigation_controller_->GoToIndex(destination_entry_index_);
  }
  destination_entry_index_ = -1;
}

void BackForwardTransitionAnimationManagerAndroid::
    OnContentForNavigationEntryShown() {
  if (animator_) {
    animator_->OnContentForNavigationEntryShown();
    MaybeDestroyAnimator();
  }
}

AnimationStage
BackForwardTransitionAnimationManagerAndroid::GetCurrentAnimationStage() {
  return animator_ ? animator_->GetCurrentAnimationStage()
                   : AnimationStage::kNone;
}

void BackForwardTransitionAnimationManagerAndroid::OnDetachedFromWindow() {
  // The WebContentsViewAndroid's native view is detached from the top level
  // window. We must abort the transition.
  CHECK(animator_);
  animator_->AbortAnimation();
  DestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::
    OnRootWindowVisibilityChanged(bool visible) {
  CHECK(animator_);
  if (!visible) {
    animator_->AbortAnimation();
    DestroyAnimator();
  }
}

void BackForwardTransitionAnimationManagerAndroid::OnDetachCompositor() {
  CHECK(animator_);
  animator_->AbortAnimation();
  DestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::OnAnimate(
    base::TimeTicks frame_begin_time) {
  animator_->OnAnimate(frame_begin_time);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::RenderWidgetHostDestroyed(
    RenderWidgetHost* widget_host) {
  animator_->OnRenderWidgetHostDestroyed(widget_host);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::
    OnRenderFrameMetadataChangedAfterActivation(
        base::TimeTicks activation_time) {
  animator_->OnRenderFrameMetadataChangedAfterActivation(activation_time);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  animator_->DidStartNavigation(navigation_handle);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  animator_->DidFinishNavigation(navigation_handle);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  animator_->ReadyToCommitNavigation(navigation_handle);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::
    OnDidNavigatePrimaryMainFramePreCommit(
        NavigationRequest* navigation_request,
        RenderFrameHostImpl* old_host,
        RenderFrameHostImpl* new_host) {
  if (animator_) {
    animator_->OnDidNavigatePrimaryMainFramePreCommit(navigation_request,
                                                      old_host, new_host);
    MaybeDestroyAnimator();
  }
}

void BackForwardTransitionAnimationManagerAndroid::
    OnNavigationCancelledBeforeStart(NavigationHandle* navigation_handle) {
  if (animator_) {
    animator_->OnNavigationCancelledBeforeStart(navigation_handle);
    MaybeDestroyAnimator();
  }
}

void BackForwardTransitionAnimationManagerAndroid::OnAnimationStageChanged() {
  web_contents_view_android()
      ->web_contents()
      ->GetDelegate()
      ->DidBackForwardTransitionAnimationChange();
}

void BackForwardTransitionAnimationManagerAndroid::MaybeDestroyAnimator() {
  CHECK(animator_);
  if (animator_->IsTerminalState()) {
    DestroyAnimator();
  }
}

void BackForwardTransitionAnimationManagerAndroid::DestroyAnimator() {
  CHECK(animator_);
  WebContentsObserver::Observe(nullptr);
  auto* window = web_contents_view_android()->GetTopLevelNativeWindow();
  CHECK(window);
  window->RemoveObserver(this);
  web_contents_view_android()->GetNativeView()->RemoveObserver(this);
  animator_.reset();
  OnAnimationStageChanged();
}

}  // namespace content

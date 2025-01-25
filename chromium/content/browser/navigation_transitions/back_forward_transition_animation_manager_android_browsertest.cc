// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"

#include <string_view>

#include "base/android/scoped_java_ref.h"
#include "base/numerics/ranges.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/layer_tree_impl.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/test/pixel_test_utils.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/navigation_transitions/back_forward_transition_animator.h"
#include "content/browser/navigation_transitions/physics_model.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/navigation_transition_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/update_user_activation_state_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "ui/android/progress_bar_config.h"
#include "ui/android/ui_android_features.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/events/back_gesture_event.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace content {

namespace {

using SwipeEdge = ui::BackGestureEventSwipeEdge;
using NavType = BackForwardTransitionAnimationManager::NavigationDirection;

// The tolerance for two float to be considered equal.
static constexpr float kFloatTolerance = 0.001f;

// TODO(liuwilliam): 99 seconds seems arbitrary. Pick a meaningful constant
// instead.
// If the duration is long enough, the spring will return the final (rest /
// equilibrium) position right away. This means each spring model will just
// produce one frame: the frame for the final position.
constexpr base::TimeDelta kLongDurationBetweenFrames = base::Seconds(99);

enum class GestureType {
  kStart,
  // 30/60/90 are the gesture progresses.
  k30ViewportWidth,
  k60ViewportWidth,
  k90ViewportWidth,
  kCancel,
  kInvoke,
};

struct LayerTransforms {
  gfx::Transform active_page;
  std::optional<gfx::Transform> screenshot;
};

static constexpr LayerTransforms kActivePageAtOrigin{
    .active_page = gfx::Transform::MakeTranslation(0.f, 0.f),
    .screenshot = std::nullopt};

static constexpr LayerTransforms kBothLayersCentered{
    .active_page = gfx::Transform::MakeTranslation(0.f, 0.f),
    .screenshot = gfx::Transform::MakeTranslation(0.f, 0.f)};

bool TwoSkColorApproximatelyEqual(const SkColor4f& a, const SkColor4f& b) {
  return base::IsApproximatelyEqual(a.fA, b.fA, kFloatTolerance) &&
         base::IsApproximatelyEqual(a.fB, b.fB, kFloatTolerance) &&
         base::IsApproximatelyEqual(a.fG, b.fG, kFloatTolerance) &&
         base::IsApproximatelyEqual(a.fR, b.fR, kFloatTolerance);
}

SkColor4f GetScrimForGestureProgress(GestureType gesture) {
  auto scrim = SkColors::kBlack;
  switch (gesture) {
    case GestureType::kStart:
      scrim.fA = 0.8f;
      break;
    case GestureType::k30ViewportWidth:
      scrim.fA = 0.6725f;
      break;
    case GestureType::k60ViewportWidth:
      scrim.fA = 0.545f;
      break;
    case GestureType::k90ViewportWidth:
      scrim.fA = 0.4175f;
      break;
    case GestureType::kCancel:
    case GestureType::kInvoke:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return scrim;
}

BackForwardTransitionAnimationManagerAndroid* GetAnimationManager(
    WebContents* tab) {
  auto* manager = tab->GetBackForwardTransitionAnimationManager();
  EXPECT_TRUE(manager);
  return static_cast<BackForwardTransitionAnimationManagerAndroid*>(manager);
}

float GetProgress(GestureType gesture) {
  switch (gesture) {
    case GestureType::kStart:
      return 0.f;
    case GestureType::k30ViewportWidth:
      return 0.3f;
    case GestureType::k60ViewportWidth:
      return 0.6f;
    case GestureType::k90ViewportWidth:
      return 0.9f;
    case GestureType::kCancel:
    case GestureType::kInvoke:
      return -1.0f;
  }
}

int64_t GetItemSequenceNumberForNavigation(
    NavigationHandle* navigation_handle) {
  auto* request = static_cast<NavigationRequest*>(navigation_handle);
  EXPECT_TRUE(request->GetNavigationEntry());
  EXPECT_TRUE(request->GetRenderFrameHost());
  return static_cast<NavigationEntryImpl*>(request->GetNavigationEntry())
      ->GetFrameEntry(request->GetRenderFrameHost()->frame_tree_node())
      ->item_sequence_number();
}

// Assert that the layers directly owned by the WebContents's native view have
// the transform `transforms`.
enum class CrossFadeOrOldSurfaceClone {
  kNoCrossfadeNoSurfaceClone,
  kCrossfade,
  kSurfaceClone,
};
void ExpectedLayerTransforms(
    WebContentsImpl* web_contents,
    const LayerTransforms& transforms,
    CrossFadeOrOldSurfaceClone crossfade_or_clone =
        CrossFadeOrOldSurfaceClone::kNoCrossfadeNoSurfaceClone) {
  const auto& layers =
      static_cast<WebContentsViewAndroid*>(web_contents->GetView())
          ->GetNativeView()
          ->GetLayer()
          ->children();
  if (!transforms.screenshot.has_value()) {
    ASSERT_EQ(layers.size(), 1u);
    ASSERT_EQ(layers[0].get(), GetAnimationManager(web_contents)
                                   ->web_contents_view_android()
                                   ->parent_for_web_page_widgets());
    auto actual = layers[0]->transform();
    EXPECT_TRANSFORM_NEAR(actual, transforms.active_page, kFloatTolerance)
        << "Active page: actual " << actual.ToString() << " expected "
        << transforms.active_page.ToString();
  } else {
    size_t screenshot_index = 0u;
    size_t active_page_index = 0u;
    size_t old_surface_clone_index = 0u;
    switch (crossfade_or_clone) {
      case CrossFadeOrOldSurfaceClone::kNoCrossfadeNoSurfaceClone: {
        ASSERT_EQ(layers.size(), 2u);
        screenshot_index = 0u;
        active_page_index = 1u;
        break;
      }
      case CrossFadeOrOldSurfaceClone::kCrossfade: {
        ASSERT_EQ(layers.size(), 2u);
        screenshot_index = 1u;
        active_page_index = 0u;
        break;
      }
      case CrossFadeOrOldSurfaceClone::kSurfaceClone: {
        ASSERT_EQ(layers.size(), 3u);
        screenshot_index = 0u;
        active_page_index = 1u;
        old_surface_clone_index = 2u;
        break;
      }
    }
    ASSERT_EQ(layers[active_page_index].get(),
              GetAnimationManager(web_contents)
                  ->web_contents_view_android()
                  ->parent_for_web_page_widgets());
    auto actual_screenshot = layers[screenshot_index]->transform();
    EXPECT_TRANSFORM_NEAR(actual_screenshot, transforms.screenshot.value(),
                          kFloatTolerance)
        << "Screenshot: actual " << actual_screenshot.ToString() << " expected "
        << transforms.screenshot->ToString();
    auto actual_active_page = layers[active_page_index]->transform();
    EXPECT_TRANSFORM_NEAR(actual_active_page, transforms.active_page,
                          kFloatTolerance)
        << "Active page: actual " << actual_active_page.ToString()
        << " expected " << transforms.active_page.ToString();
    if (crossfade_or_clone == CrossFadeOrOldSurfaceClone::kSurfaceClone) {
      EXPECT_TRANSFORM_NEAR(layers[old_surface_clone_index]->transform(),
                            transforms.active_page, kFloatTolerance);
    }
  }
}

class AnimatorForTesting : public BackForwardTransitionAnimator {
 public:
  explicit AnimatorForTesting(
      WebContentsViewAndroid* web_contents_view_android,
      NavigationControllerImpl* controller,
      const ui::BackGestureEvent& gesture,
      BackForwardTransitionAnimationManager::NavigationDirection nav_type,
      ui::BackGestureEventSwipeEdge initiating_edge,
      NavigationEntryImpl* destination_entry,
      BackForwardTransitionAnimationManagerAndroid* animation_manager)
      : BackForwardTransitionAnimator(web_contents_view_android,
                                      controller,
                                      gesture,
                                      nav_type,
                                      initiating_edge,
                                      destination_entry,
                                      animation_manager),
        wcva_(web_contents_view_android) {}

  ~AnimatorForTesting() override {
    if (on_impl_destroyed_) {
      std::move(on_impl_destroyed_).Run();
    }
    ExpectState(finished_state_);
  }

  // `BackForwardTransitionAnimator`:
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override {
    if (intercept_render_frame_metadata_changed_) {
      return;
    }
    if (state() == State::kWaitingForNewRendererToDraw &&
        waited_for_renderer_new_frame_) {
      std::move(waited_for_renderer_new_frame_).Run();
    }

    BackForwardTransitionAnimator::OnRenderFrameMetadataChangedAfterActivation(
        activation_time);

    if (state() == State::kDisplayingCrossFadeAnimation) {
      ExpectedLayerTransforms(wcva_->web_contents(), kBothLayersCentered,
                              CrossFadeOrOldSurfaceClone::kCrossfade);
    }
  }
  void OnAnimate(base::TimeTicks frame_begin_time) override {
    if (state() == State::kDisplayingCrossFadeAnimation &&
        !seen_first_on_animate_for_cross_fade_) {
      seen_first_on_animate_for_cross_fade_ = true;
      ExpectedLayerTransforms(wcva_->web_contents(), kBothLayersCentered,
                              CrossFadeOrOldSurfaceClone::kCrossfade);
      const auto& layers = GetChildrenLayersOfWebContentsView();
      // The first OnAnimate for the cross-fade animation will set the scrim
      // to 0.3, and opacity to 1.
      ASSERT_EQ(layers.at(1)->children().size(), 1U);
      ASSERT_EQ(layers.at(1)->children().at(0)->background_color().fA, 0.3f);
      ASSERT_EQ(layers.at(1)->opacity(), 1.f);
    }
    if (pause_on_animate_at_state_.has_value() &&
        pause_on_animate_at_state_.value() == state()) {
      return;
    }
    if (next_on_animate_callback_) {
      std::move(next_on_animate_callback_).Run();
    }
    static base::TimeTicks tick = base::TimeTicks();
    tick += duration_between_frames_;
    BackForwardTransitionAnimator::OnAnimate(tick);
  }
  void OnCancelAnimationDisplayed() override {
    if (on_cancel_animation_displayed_) {
      std::move(on_cancel_animation_displayed_).Run();
    }
    float full_width_offset =
        wcva_->GetNativeView()->GetPhysicalBackingSize().width();
    if (initiating_edge() == SwipeEdge::RIGHT) {
      full_width_offset *= -1;
    }
    static LayerTransforms on_cancelled{
        .active_page = gfx::Transform::MakeTranslation(0.f, 0.f),
        .screenshot = gfx::Transform::MakeTranslation(
            full_width_offset * PhysicsModel::kScreenshotInitialPositionRatio,
            0.f)};
    ExpectedLayerTransforms(wcva_->web_contents(), on_cancelled);

    const auto& layers = GetChildrenLayersOfWebContentsView();
    ASSERT_EQ(layers.size(), 2U);
    ASSERT_EQ(layers.at(0)->children().size(), 1U);
    // Screenshot should have the scrim.
    EXPECT_EQ(layers.at(0)->children().at(0)->background_color().fA, 0.8f);

    BackForwardTransitionAnimator::OnCancelAnimationDisplayed();
  }
  void OnInvokeAnimationDisplayed() override {
    if (on_invoke_animation_displayed_) {
      std::move(on_invoke_animation_displayed_).Run();
    }
    float full_width_offset =
        wcva_->GetNativeView()->GetPhysicalBackingSize().width();
    if (initiating_edge() == SwipeEdge::RIGHT) {
      full_width_offset *= -1;
    }
    static LayerTransforms on_invoked{
        .active_page = gfx::Transform::MakeTranslation(full_width_offset, 0.f),
        .screenshot = gfx::Transform::MakeTranslation(0.f, 0.f)};
    // There won't be a old surface clone if the navigation is from a crashed
    // page.
    if (navigating_from_a_crashed_page_) {
      ExpectedLayerTransforms(wcva_->web_contents(), on_invoked);
    } else {
      ExpectedLayerTransforms(wcva_->web_contents(), on_invoked,
                              CrossFadeOrOldSurfaceClone::kSurfaceClone);
    }

    const auto& layers = GetChildrenLayersOfWebContentsView();
    ASSERT_EQ(layers.size(), navigating_from_a_crashed_page_ ? 2U : 3U);

    const bool has_progress_bar = wcva_->GetNativeView()
                                      ->GetWindowAndroid()
                                      ->GetProgressBarConfig()
                                      .ShouldDisplay();

    const auto& screenshot_layer = layers.at(0);
    ASSERT_EQ(screenshot_layer->children().size(), has_progress_bar ? 2u : 1u);
    // Scrim should be at the end of the first timeline.
    EXPECT_EQ(screenshot_layer->children().at(0)->background_color().fA, 0.3f);

    BackForwardTransitionAnimator::OnInvokeAnimationDisplayed();

    if (state() == State::kDisplayingCrossFadeAnimation) {
      ExpectedLayerTransforms(wcva_->web_contents(), kBothLayersCentered,
                              CrossFadeOrOldSurfaceClone::kCrossfade);
    }
  }
  void OnCrossFadeAnimationDisplayed() override {
    if (on_cross_fade_animation_displayed_) {
      std::move(on_cross_fade_animation_displayed_).Run();
    }

    // Both layers are centered to display the cross-fade.
    ExpectedLayerTransforms(wcva_->web_contents(), kBothLayersCentered,
                            CrossFadeOrOldSurfaceClone::kCrossfade);

    const auto& layers = GetChildrenLayersOfWebContentsView();
    ASSERT_EQ(layers.size(), 2U);

    // Opacities for cross-fade.
    // Active page.
    ASSERT_EQ(layers.at(0)->opacity(), 1.f);
    // Screenshot page.
    ASSERT_EQ(layers.at(1)->opacity(), 0.f);

    // Screenshot shouldn't have any scrim over it.
    ASSERT_EQ(layers.at(1)->children().size(), 1U);
    EXPECT_EQ(layers.at(1)->children().at(0)->background_color().fA, 0.f);

    BackForwardTransitionAnimator::OnCrossFadeAnimationDisplayed();
  }
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    last_navigation_request_ =
        NavigationRequest::From(navigation_handle)->GetWeakPtr();
    BackForwardTransitionAnimator::DidStartNavigation(navigation_handle);
  }
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    BackForwardTransitionAnimator::ReadyToCommitNavigation(navigation_handle);
    if (post_ready_to_commit_callback_) {
      std::move(post_ready_to_commit_callback_).Run();
    }
  }
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (did_finish_navigation_callback_) {
      std::move(did_finish_navigation_callback_).Run();
    }
    BackForwardTransitionAnimator::DidFinishNavigation(navigation_handle);
  }

  NavigationRequest* LastNavigationRequest() {
    CHECK(last_navigation_request_);
    return last_navigation_request_.get();
  }
  void PauseAnimationAtDisplayingCancelAnimation() {
    ASSERT_FALSE(pause_on_animate_at_state_.has_value()) << "Already paused.";
    pause_on_animate_at_state_ = State::kDisplayingCancelAnimation;
  }

  void PauseAnimationAtDisplayingInvokeAnimation() {
    ASSERT_FALSE(pause_on_animate_at_state_.has_value()) << "Already paused.";
    pause_on_animate_at_state_ = State::kDisplayingInvokeAnimation;
  }

  void PauseAnimationAtDisplayingCrossFadeAnimation() {
    ASSERT_FALSE(pause_on_animate_at_state_.has_value()) << "Already paused.";
    pause_on_animate_at_state_ = State::kDisplayingCrossFadeAnimation;
  }

  void UnpauseAnimation() {
    pause_on_animate_at_state_ = std::nullopt;
    OnAnimate(base::TimeTicks{});
  }

  void ExpectWaitingForNewFrame() {
    ExpectState(State::kWaitingForNewRendererToDraw);
  }

  void ExpectDisplayingInvokeAnimation() {
    ExpectState(State::kDisplayingInvokeAnimation);
  }

  void ExpectDisplayingCancelAnimation() {
    ExpectState(State::kDisplayingCancelAnimation);
  }

  void ExpectWaitingForBeforeUnloadResponse() {
    ExpectState(State::kWaitingForBeforeUnloadResponse);
  }

  void ExpectWaitingForDisplayingCrossFadeAnimation() {
    ExpectState(State::kDisplayingCrossFadeAnimation);
  }

  void SetFinishedStateToAnimationAborted() {
    finished_state_ = State::kAnimationAborted;
  }

  void set_intercept_render_frame_metadata_changed(bool intercept) {
    intercept_render_frame_metadata_changed_ = intercept;
  }
  void set_on_cancel_animation_displayed(base::OnceClosure callback) {
    ASSERT_FALSE(on_cancel_animation_displayed_);
    on_cancel_animation_displayed_ = std::move(callback);
  }
  void set_on_invoke_animation_displayed(base::OnceClosure callback) {
    ASSERT_FALSE(on_invoke_animation_displayed_);
    on_invoke_animation_displayed_ = std::move(callback);
  }
  void set_on_cross_fade_animation_displayed(base::OnceClosure callback) {
    ASSERT_FALSE(on_cross_fade_animation_displayed_);
    on_cross_fade_animation_displayed_ = std::move(callback);
  }
  void set_waited_for_renderer_new_frame(base::OnceClosure callback) {
    ASSERT_FALSE(waited_for_renderer_new_frame_);
    waited_for_renderer_new_frame_ = std::move(callback);
  }
  void set_next_on_animate_callback(base::OnceClosure callback) {
    ASSERT_FALSE(next_on_animate_callback_);
    next_on_animate_callback_ = std::move(callback);
  }
  void set_post_ready_to_commit_callback(base::OnceClosure callback) {
    ASSERT_FALSE(post_ready_to_commit_callback_);
    post_ready_to_commit_callback_ = std::move(callback);
  }
  void set_did_finish_navigation_callback(base::OnceClosure callback) {
    ASSERT_FALSE(did_finish_navigation_callback_);
    did_finish_navigation_callback_ = std::move(callback);
  }
  void set_on_impl_destroyed(base::OnceClosure callback) {
    ASSERT_FALSE(on_impl_destroyed_);
    on_impl_destroyed_ = std::move(callback);
  }
  void set_duration_between_frames(base::TimeDelta duration) {
    duration_between_frames_ = duration;
  }
  void set_navigating_from_a_crashed_page(bool navigating_from_a_crashed_page) {
    navigating_from_a_crashed_page_ = navigating_from_a_crashed_page;
  }

 private:
  void ExpectState(State expected) const {
    EXPECT_EQ(state(), expected)
        << ToString(state()) << " vs " << ToString(expected);
  }

  const std::vector<scoped_refptr<cc::slim::Layer>>&
  GetChildrenLayersOfWebContentsView() const {
    return static_cast<WebContentsViewAndroid*>(
               wcva_->web_contents()->GetView())
        ->GetNativeView()
        ->GetLayer()
        ->children();
  }

  const raw_ptr<WebContentsViewAndroid> wcva_;

  base::TimeDelta duration_between_frames_ = kLongDurationBetweenFrames;

  // By default, the test should expect the animator has successfully finished.
  // Use `SetFinishedStateTo*()` to change this expectation.
  State finished_state_ = State::kAnimationFinished;

  bool intercept_render_frame_metadata_changed_ = false;

  bool seen_first_on_animate_for_cross_fade_ = false;

  bool navigating_from_a_crashed_page_ = false;

  std::optional<State> pause_on_animate_at_state_;

  base::WeakPtr<NavigationRequest> last_navigation_request_;

  base::OnceClosure on_cancel_animation_displayed_;
  base::OnceClosure on_invoke_animation_displayed_;
  base::OnceClosure on_cross_fade_animation_displayed_;
  base::OnceClosure waited_for_renderer_new_frame_;
  base::OnceClosure next_on_animate_callback_;
  base::OnceClosure post_ready_to_commit_callback_;
  base::OnceClosure did_finish_navigation_callback_;
  base::OnceClosure on_impl_destroyed_;
};

class FactoryForTesting : public BackForwardTransitionAnimator::Factory {
 public:
  FactoryForTesting() = default;
  ~FactoryForTesting() override = default;

  std::unique_ptr<BackForwardTransitionAnimator> Create(
      WebContentsViewAndroid* web_contents_view_android,
      NavigationControllerImpl* controller,
      const ui::BackGestureEvent& gesture,
      BackForwardTransitionAnimationManager::NavigationDirection nav_type,
      ui::BackGestureEventSwipeEdge initiating_edge,
      NavigationEntryImpl* destination_entry,
      BackForwardTransitionAnimationManagerAndroid* animation_manager)
      override {
    return std::make_unique<AnimatorForTesting>(
        web_contents_view_android, controller, gesture, nav_type,
        initiating_edge, destination_entry, animation_manager);
  }
};
}  // namespace

// TODO(https://crbug.com/325329998): Enable the pixel comparison so the tests
// are truly end-to-end.
class BackForwardTransitionAnimationManagerBrowserTest
    : public ContentBrowserTest {
 public:
  BackForwardTransitionAnimationManagerBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kBackForwardTransitions, {}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
  }
  ~BackForwardTransitionAnimationManagerBrowserTest() override = default;

  void SetUp() override {
    if (base::SysInfo::GetAndroidHardwareEGL() == "emulation") {
      // crbug.com/337886037 and crrev.com/c/5504854/comment/b81b8fb6_95fb1381/:
      // The CopyOutputRequests crash the GPU process. ANGLE is exporting the
      // native fence support on Android emulators but it doesn't work properly.
      GTEST_SKIP();
    }
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(
        base::FeatureList::IsEnabled(blink::features::kBackForwardTransitions));

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Manually load a "red" document because we are still at the initial
    // entry.
    ASSERT_TRUE(NavigateToURL(web_contents(), RedURL()));
    WaitForCopyableViewInWebContents(web_contents());

    auto* manager =
        BrowserContextImpl::From(web_contents()->GetBrowserContext())
            ->GetNavigationEntryScreenshotManager();
    ASSERT_TRUE(manager);
    ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
    ASSERT_TRUE(web_contents()->GetRenderWidgetHostView());
    // 10 Screenshots, with 4 bytes per screenshot.
    manager->SetMemoryBudgetForTesting(4 * GetViewportSize().Area64() * 10);

    // Set up for a backward navigation: [red&, green*].
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(NavigateToURL(web_contents(), GreenURL()));
    observer.Wait();
    WaitForCopyableViewInWebContents(web_contents());

    auto* animation_manager = GetAnimationManager(web_contents());
    animation_manager->set_animator_factory_for_testing(
        std::make_unique<FactoryForTesting>());
  }

  gfx::Size GetViewportSize() {
    return web_contents()->GetNativeView()->GetPhysicalBackingSize();
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  virtual SwipeEdge GetSwipeEdge() const { return SwipeEdge::LEFT; }

  GURL RedURL() const { return embedded_test_server()->GetURL("/red.html"); }

  GURL GreenURL() const {
    return embedded_test_server()->GetURL("/green.html");
  }

  GURL BlueURL() const { return embedded_test_server()->GetURL("/blue.html"); }

  LayerTransforms GetLayerTransformsForGestureProgress(GestureType gesture) {
    float direction_constant = GetSwipeEdge() == SwipeEdge::LEFT ? 1.f : -1.f;
    int width = GetViewportSize().width();
    float commit_pending =
        width * PhysicsModel::kTargetCommitPendingRatio * direction_constant;
    float screenshot_initial = width *
                               PhysicsModel::kScreenshotInitialPositionRatio *
                               direction_constant;
    switch (gesture) {
      case GestureType::kStart:
        return {.active_page = gfx::Transform::MakeTranslation(0.f, 0.f),
                .screenshot =
                    gfx::Transform::MakeTranslation(screenshot_initial, 0.f)};
      case GestureType::k30ViewportWidth:
        return {.active_page =
                    gfx::Transform::MakeTranslation(commit_pending * 0.3f, 0.f),
                .screenshot = gfx::Transform::MakeTranslation(
                    screenshot_initial * 0.7f, 0.f)};
      case GestureType::k60ViewportWidth:
        return {.active_page =
                    gfx::Transform::MakeTranslation(commit_pending * 0.6f, 0.f),
                .screenshot = gfx::Transform::MakeTranslation(
                    screenshot_initial * 0.4f, 0.f)};
      case GestureType::k90ViewportWidth:
        return {.active_page =
                    gfx::Transform::MakeTranslation(commit_pending * 0.9f, 0.f),
                .screenshot = gfx::Transform::MakeTranslation(
                    screenshot_initial * 0.1f, 0.f)};
      case GestureType::kCancel:
      case GestureType::kInvoke:
        NOTREACHED_NORETURN();
    }
  }

  // Perform a history back navigation by sending the specified gesture events.
  // Checks that the content in the viewport matches the expectations.
  void HistoryBackNavAndAssertAnimatedTransition(
      const std::vector<GestureType>& gestures) {
    auto* animation_manager = GetAnimationManager(web_contents());

    for (const auto& gesture : gestures) {
      switch (gesture) {
        case GestureType::kStart: {
          SCOPED_TRACE("kStart");
          ProgressGestureAndExpectTransformAndScrim(gesture);
          ASSERT_FALSE(
              web_contents()->GetController().GetActiveEntry()->GetUserData(
                  NavigationEntryScreenshot::kUserDataKey));
          break;
        }
        case GestureType::k30ViewportWidth: {
          SCOPED_TRACE("k30ViewportWidth");
          ProgressGestureAndExpectTransformAndScrim(gesture);
          break;
        }
        case GestureType::k60ViewportWidth: {
          SCOPED_TRACE("k60ViewportWidth");
          ProgressGestureAndExpectTransformAndScrim(gesture);
          break;
        }
        case GestureType::k90ViewportWidth: {
          SCOPED_TRACE("k90ViewportWidth");
          ProgressGestureAndExpectTransformAndScrim(gesture);
          break;
        }
        case GestureType::kCancel: {
          SCOPED_TRACE("kCancel");
          // Use a RunLoop because the animation runs asynchronously at the next
          // BeginFrame.
          base::RunLoop cancel_played;
          GetAnimatorForTesting()->set_on_cancel_animation_displayed(
              cancel_played.QuitClosure());
          animation_manager->OnGestureCancelled();
          cancel_played.Run();
          break;
        }
        case GestureType::kInvoke: {
          SCOPED_TRACE("kInvoke");
          // Use a RunLoop because the animation runs asynchronously at the next
          // BeginFrame.
          base::RunLoop invoke_played;
          GetAnimatorForTesting()->set_on_invoke_animation_displayed(
              invoke_played.QuitClosure());
          animation_manager->OnGestureInvoked();
          invoke_played.Run();
          break;
        }
      }
    }
  }

  void ProgressGestureAndExpectTransformAndScrim(GestureType gesture) {
    // TODO(bokan): The touch location isn't currently used but ideally we'd
    // send realistic values for the location too. (Or can we remove it?)
    const gfx::PointF touch_pt(1, 1);
    const float progress = GetProgress(gesture);

    if (gesture == GestureType::kStart) {
      GetAnimationManager(web_contents())
          ->OnGestureStarted(ui::BackGestureEvent(touch_pt, progress),
                             GetSwipeEdge(), NavType::kBackward);
    } else {
      GetAnimationManager(web_contents())
          ->OnGestureProgressed(ui::BackGestureEvent(touch_pt, progress));
    }
    ExpectLayerTransformsAndScrimForGestureProgress(gesture);
  }

  void ExpectLayerTransformsAndScrimForGestureProgress(GestureType gesture) {
    ExpectedLayerTransforms(web_contents(),
                            GetLayerTransformsForGestureProgress(gesture));
    const auto& screenshot_layer = GetScreenshotLayer();
    // The screenshot must have the scrim layer as a child.
    ASSERT_EQ(screenshot_layer->children().size(), 1U);
    SkColor4f actual =
        screenshot_layer->children().at(0).get()->background_color();
    SkColor4f expected = GetScrimForGestureProgress(gesture);
    EXPECT_TRUE(TwoSkColorApproximatelyEqual(actual, expected))
        << "actual " << actual.fA << " expected " << expected.fA;
  }

  scoped_refptr<cc::slim::Layer> GetScreenshotLayer() {
    const auto& layers =
        static_cast<WebContentsViewAndroid*>(web_contents()->GetView())
            ->GetNativeView()
            ->GetLayer()
            ->children();
    // The first layer is the screenshot.
    return layers[0];
  }

  AnimatorForTesting* GetAnimatorForTesting() {
    auto* manager = static_cast<BackForwardTransitionAnimationManagerAndroid*>(
        web_contents()->GetBackForwardTransitionAnimationManager());
    EXPECT_TRUE(manager);
    auto* animator = static_cast<AnimatorForTesting*>(manager->animator_.get());
    EXPECT_TRUE(animator) << "Can only be called after a gesture has started.";
    return animator;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Basic tests which will be run both with a swipe from the left edge as well as
// a swipe from the right edge with an RTL UI direction. Tests from the right
// edge also force the UI to use an RTL direction.
class BackForwardTransitionAnimationManagerBothEdgeBrowserTest
    : public BackForwardTransitionAnimationManagerBrowserTest,
      public ::testing::WithParamInterface<SwipeEdge> {
 public:
  BackForwardTransitionAnimationManagerBothEdgeBrowserTest() {
    scoped_feature_list_.Reset();
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kBackForwardTransitions, {}},
        {ui::kMirrorBackForwardGesturesInRTL, {}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
  }
  ~BackForwardTransitionAnimationManagerBothEdgeBrowserTest() override =
      default;

  void SetUp() override {
    if (GetParam() == SwipeEdge::RIGHT) {
      l10n_util::SetRtlForTesting(true);
    }

    BackForwardTransitionAnimationManagerBrowserTest::SetUp();
  }

  SwipeEdge GetSwipeEdge() const override { return GetParam(); }
};

// Simulates the gesture sequence: start, 30%, 60%, 90%, 60%, 30%, 60%, 90% and
// finally invoke.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       Invoke) {
  // Back nav from the green page to the red page. The live page (green) is on
  // top and slides towards right. The red page (screenshot) is on the bottom
  // and appears on the left of screen.
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k30ViewportWidth);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::k90ViewportWidth);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::k30ViewportWidth);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::k90ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  // Manually trigger the back navigation to wait for the animations to fully
  // finish. Waiting for the navigation's finish to terminate the test is flaky
  // because the invoke animation can sill be running when the navigation
  // finishes.
  TestFrameNavigationObserver back_to_red(web_contents());
  base::RunLoop cross_fade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  GetAnimationManager(web_contents())->OnGestureInvoked();
  cross_fade_displayed.Run();
  destroyed.Run();
  back_to_red.Wait();

  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// Simulates the gesture sequence: start, 30%, 60%, 90%, 60%, 30% and finally
// cancels.
IN_PROC_BROWSER_TEST_P(BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
                       Cancel) {
  // Back nav from the green page to the red page. The live page (green) is on
  // top and slides towards right. The red page (screenshot) is on the bottom
  // and appears on the left of screen.
  std::vector<GestureType> expected;

  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k30ViewportWidth);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::k90ViewportWidth);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::k30ViewportWidth);
  expected.push_back(GestureType::kCancel);

  HistoryBackNavAndAssertAnimatedTransition(expected);
  ASSERT_EQ(web_contents()->GetController().GetActiveEntry()->GetURL(),
            GreenURL());
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            RedURL());
  ASSERT_TRUE(web_contents()->GetController().GetEntryAtIndex(0)->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardTransitionAnimationManagerBothEdgeBrowserTest,
    ::testing::Values(SwipeEdge::LEFT, SwipeEdge::RIGHT),
    [](const testing::TestParamInfo<
        BackForwardTransitionAnimationManagerBothEdgeBrowserTest::ParamType>&
           info) {
      return info.param == SwipeEdge::LEFT ? "LeftEdge" : "RightEdge";
    });

// Runs a transition in a ViewTransition enabled page. Ensures view transition
// does not run.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       DefaultTransitionSupersedesViewTransition) {
  GURL test_url(
      embedded_test_server()->GetURL("/view_transitions/basic-vt-opt-in.html"));
  ASSERT_TRUE(NavigateToURL(web_contents(), test_url));
  WaitForCopyableViewInWebContents(web_contents());

  GURL test_url_next(embedded_test_server()->GetURL(
      "/view_transitions/basic-vt-opt-in.html?next"));
  ASSERT_TRUE(NavigateToURL(web_contents(), test_url_next));
  WaitForCopyableViewInWebContents(web_contents());

  // Back nav from the green page to the red page. The live page (green) is on
  // top and slides towards right. The red page (screenshot) is on the bottom
  // and appears on the left of screen.
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k30ViewportWidth);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::k90ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  // Manually trigger the back navigation.
  TestFrameNavigationObserver back_navigation(web_contents());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  GetAnimationManager(web_contents())->OnGestureInvoked();
  destroyed.Run();
  back_navigation.Wait();

  // Ensure the new Document has produced a frame, otherwise `pagereveal` which
  // sets had_incoming_transition might not have been fired yet.
  WaitForCopyableViewInWebContents(web_contents());

  ASSERT_EQ(back_navigation.last_committed_url(), test_url);
  EXPECT_EQ(false, EvalJs(web_contents(), "had_incoming_transition"));
}

// If the destination has no screenshot, we will compose a fallback screenshot
// for transition.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       DestinationHasNoScreenshot) {
  std::optional<int> index =
      web_contents()->GetController().GetIndexForGoBack();
  ASSERT_TRUE(index);
  auto* red_entry = web_contents()->GetController().GetEntryAtIndex(*index);
  ASSERT_TRUE(web_contents()
                  ->GetController()
                  .GetNavigationEntryScreenshotCache()
                  ->RemoveScreenshot(red_entry));
  red_entry->navigation_transition_data().set_main_frame_background_color(
      SkColors::kMagenta);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  const auto& children =
      static_cast<WebContentsViewAndroid*>(web_contents()->GetView())
          ->parent_for_web_page_widgets()
          ->parent()
          ->children();
  // `parent_for_web_page_widgets()` and the screenshot.
  ASSERT_EQ(children.size(), 2U);
  auto* fallback_screenshot =
      static_cast<cc::slim::SolidColorLayer*>(children[0].get());
  ASSERT_EQ(fallback_screenshot->background_color(), SkColors::kMagenta);

  // Manually trigger the back navigation.
  TestFrameNavigationObserver back_navigation(web_contents());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  GetAnimationManager(web_contents())->OnGestureInvoked();
  destroyed.Run();
  back_navigation.Wait();

  ASSERT_EQ(back_navigation.last_committed_url(), RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// Assert that if the user does not start the navigation, we don't put the
// fallback screenshot back.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       Cancel_DestinationNoScreenshot) {
  std::optional<int> index =
      web_contents()->GetController().GetIndexForGoBack();
  ASSERT_TRUE(index);
  auto* red_entry = web_contents()->GetController().GetEntryAtIndex(*index);
  ASSERT_TRUE(web_contents()
                  ->GetController()
                  .GetNavigationEntryScreenshotCache()
                  ->RemoveScreenshot(red_entry));

  std::vector<GestureType> expected;

  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::kCancel);

  HistoryBackNavAndAssertAnimatedTransition(expected);
  ASSERT_EQ(web_contents()->GetController().GetActiveEntry()->GetURL(),
            GreenURL());
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetEntryAtIndex(0)->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// Simulating the user click the X button to cancel the navigation while the
// animation is at commit-pending.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigationAborted) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;

  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);

  // We haven't started the navigation at this point.
  HistoryBackNavAndAssertAnimatedTransition(expected);

  TestNavigationManager back_to_red(web_contents(), RedURL());
  // The user has lifted the finger - signaling the start of the navigation.
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForResponse());

  base::RunLoop cancel_played;
  GetAnimatorForTesting()->set_on_cancel_animation_displayed(
      cancel_played.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  // The user clicks the X button.
  web_contents()->Stop();
  cancel_played.Run();
  ASSERT_FALSE(back_to_red.was_committed());
  destroyed.Run();

  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);

  // [red, green*].
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            GreenURL());
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            RedURL());
  ASSERT_TRUE(web_contents()->GetController().GetEntryAtIndex(0)->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// The invoke animation is displaying and the gesture navigation is <
// READY_TO_COMMIT. A secondary navigation cancels our gesture navigation as the
// gesture navigation has not told the renderer to commit. The cancel animation
// will be placed to bring the active page back.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       GestureNavigationBeingReplaced) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  base::RunLoop cancel_played;
  GetAnimatorForTesting()->set_on_cancel_animation_displayed(
      cancel_played.QuitClosure());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  // The pause here prevents the manager from finishing the invoke animation.
  // When the navigation to blue starts, blue's navigation request will cancel
  // the red's navigation request, and the manager will get a
  // DidFinishNavigation to advance itself from `kDisplayingInvokeAnimation`
  // to `kDisplayingCancelAnimation`.
  GetAnimatorForTesting()->PauseAnimationAtDisplayingInvokeAnimation();
  GetAnimationManager(web_contents())->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForRequestStart());
  GetAnimatorForTesting()->ExpectDisplayingInvokeAnimation();
  // We can't use NavigateToURL() here. NavigateToURL will wait for the
  // current WebContents to stop loading. We have an on-going navigation here
  // so the wait will timeout.
  {
    TestNavigationManager nav_to_blue(web_contents(), BlueURL());
    web_contents()->GetController().LoadURL(
        BlueURL(), Referrer{},
        ui::PageTransitionFromInt(
            ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR |
            ui::PageTransition::PAGE_TRANSITION_TYPED),
        std::string{});
    ASSERT_TRUE(nav_to_blue.WaitForRequestStart());
    // The start of blue will advance the manager to
    // kDisplayingCancelAnimation.
    GetAnimatorForTesting()->ExpectDisplayingCancelAnimation();
    // Force the cancel animation to finish playing, by unpausing it and
    // calling OnAnimate on it.
    GetAnimatorForTesting()->UnpauseAnimation();
    cancel_played.Run();
    ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  }
  destroyed.Run();

  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
  ASSERT_FALSE(back_nav_to_red.was_committed());
}

// The user swipes across the screen while a cross-doc navigation commits. We
// destroy the animation manager synchronously.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigationWhileOnGestureProgressed) {
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);

  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  destroyed.Run();

  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

// The cancel animation is displaying while a cross-doc navigation commits. We
// destroy the animation manager synchronously.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigationWhileDisplayingCancelAnimation) {
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  GetAnimatorForTesting()->PauseAnimationAtDisplayingCancelAnimation();
  GetAnimationManager(web_contents())->OnGestureCancelled();
  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  destroyed.Run();

  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigationWhileWaitingForRendererNewFrame) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  // The user has lifted the finger - signaling the start of the navigation.
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());

  // Intercept all the `OnRenderFrameMetadataChangedAfterActivation()`s.
  GetAnimatorForTesting()->set_intercept_render_frame_metadata_changed(true);
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  invoke_played.Run();
  GetAnimatorForTesting()->ExpectWaitingForNewFrame();

  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  destroyed.Run();

  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

// Test `BackForwardTransitionAnimator::StartNavigationAndTrackRequest()`
// returns false:
// - at OnGestureStarted() there is a destination entry;
// - at OnGestureInvoked() the entry cannot be found.
// - Upon the user lifts the finger, the cancel animation should be played, and
//   no navigation committed.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NotAbleToStartNavigationOnInvoke) {
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  // Only have the active green entry after this call.
  // `StartNavigationAndTrackRequest()` will fail.
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  web_contents()->GetController().PruneAllButLastCommitted();
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 0);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            GreenURL());

  base::RunLoop cancel_played;
  GetAnimatorForTesting()->set_on_cancel_animation_displayed(
      cancel_played.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  cancel_played.Run();
  destroyed.Run();
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);

  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 0);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            GreenURL());
}

// Test that the animation manager is blocked by the renderer's impl thread
// submitting a new compostior frame.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       AnimationStaysBeforeFrameActivation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop cross_fade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  // The user has lifted the finger - signaling the start of the navigation.
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());

  // Intercept all the `OnRenderFrameMetadataChangedAfterActivation()`s.
  GetAnimatorForTesting()->set_intercept_render_frame_metadata_changed(true);
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  invoke_played.Run();

  GetAnimatorForTesting()->set_intercept_render_frame_metadata_changed(false);
  GetAnimatorForTesting()->set_waited_for_renderer_new_frame(base::BindOnce(
      [](base::OnceClosure received_frame_while_waiting) {
        std::move(received_frame_while_waiting).Run();
      },
      base::BindOnce(&AnimatorForTesting::ExpectWaitingForNewFrame,
                     base::Unretained(GetAnimatorForTesting()))));
  GetAnimatorForTesting()->OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks());
  cross_fade_displayed.Run();
  destroyed.Run();
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

// Test that the animation manager is destroyed when the visibility changes for
// that tab.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       OnVisibilityChange) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  // Pause at the beginning of the invoke animation but wait for the navigation
  // to finish, so we can guarantee to have subscribed to the new
  // RenderWidgetHost.
  GetAnimatorForTesting()->PauseAnimationAtDisplayingInvokeAnimation();
  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());

  ui::WindowAndroid* window = web_contents()->GetTopLevelNativeWindow();
  // The first two args don't matter in tests.
  window->OnVisibilityChanged(
      /*env=*/nullptr,
      /*obj=*/base::android::JavaParamRef<jobject>(nullptr),
      /*visible=*/false);
  destroyed.Run();
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

// Test that the animation manager is destroyed when the browser compositor is
// detached.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       OnDetachCompositor) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  // Pause at the beginning of the invoke animation but wait for the navigation
  // to finish, so we can guarantee to have subscribed to the new
  // RenderWidgetHost.
  GetAnimatorForTesting()->PauseAnimationAtDisplayingInvokeAnimation();
  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());

  ui::WindowAndroid* window = web_contents()->GetTopLevelNativeWindow();
  window->DetachCompositor();
  destroyed.Run();
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

// Assert that non primary main frame navigations won't cancel the ongoing
// animation.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       IgnoreNonPrimaryMainFrameNavigations) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop cross_fade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  TestNavigationManager back_to_red(web_contents(), RedURL());
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForResponse());

  // Add an iframe to the green page while the gesture is in-progress. This will
  // trigger a renderer-initiated navigation in the subframe.
  constexpr char kAddIframeScript[] = R"({
    (()=>{
        return new Promise((resolve) => {
          const frame = document.createElement('iframe');
          frame.addEventListener('load', () => {resolve();});
          frame.src = $1;
          document.body.appendChild(frame);
        });
    })();
  })";
  ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     JsReplace(kAddIframeScript, BlueURL())));

  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  invoke_played.Run();
  cross_fade_displayed.Run();
  destroyed.Run();
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);

  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 0);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            RedURL());
}

// Assert that during OnAnimate, if the current animation hasn't finish, we
// should expect a follow up OnAnimate call.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       OnAnimateIsCalled) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop cross_fade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  GetAnimatorForTesting()->set_duration_between_frames(base::Milliseconds(1));
  GetAnimationManager(web_contents())->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());
  {
    SCOPED_TRACE("first on animate call");
    base::RunLoop first_on_animate_call;
    GetAnimatorForTesting()->set_next_on_animate_callback(
        first_on_animate_call.QuitClosure());
    first_on_animate_call.Run();
    GetAnimatorForTesting()->ExpectDisplayingInvokeAnimation();
  }
  GetAnimatorForTesting()->set_duration_between_frames(
      kLongDurationBetweenFrames);
  {
    SCOPED_TRACE("second on animate call");
    base::RunLoop second_on_animate_call;
    GetAnimatorForTesting()->set_next_on_animate_callback(
        second_on_animate_call.QuitClosure());
    second_on_animate_call.Run();
  }

  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  invoke_played.Run();
  cross_fade_displayed.Run();
  destroyed.Run();
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

// Test that, when the browser receives the DidCommit message, Viz has already
// activated a render frame, we will also skip `kWaitingForNewRendererToDraw`.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       RenderFrameActivatedBeforeDidCommit) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop cross_fade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  bool received_frame_while_waiting = false;
  GetAnimatorForTesting()->set_waited_for_renderer_new_frame(
      base::BindLambdaForTesting(
          [&]() { received_frame_while_waiting = true; }));

  TestNavigationManager back_to_red(web_contents(), RedURL());
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForResponse());

  // Manually set the new frame metadata before the DidCommit message and call
  // `OnRenderFrameMetadataChangedAfterActivation()` to simulate a frame
  // activation.
  {
    RenderFrameHostImpl* red_rfh =
        GetAnimatorForTesting()->LastNavigationRequest()->GetRenderFrameHost();
    auto* new_widget_host = red_rfh->GetRenderWidgetHost();
    ASSERT_TRUE(new_widget_host);
    auto* new_view = new_widget_host->GetView();
    ASSERT_TRUE(new_view);
    cc::RenderFrameMetadata metadata;
    metadata.primary_main_frame_item_sequence_number =
        GetItemSequenceNumberForNavigation(back_to_red.GetNavigationHandle());
    GetAnimatorForTesting()->set_post_ready_to_commit_callback(
        base::BindLambdaForTesting([&]() {
          new_widget_host->render_frame_metadata_provider()
              ->SetLastRenderFrameMetadataForTest(std::move(metadata));
          GetAnimatorForTesting()->OnRenderFrameMetadataChangedAfterActivation(
              base::TimeTicks());
        }));
  }

  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  cross_fade_displayed.Run();
  destroyed.Run();
  ASSERT_FALSE(received_frame_while_waiting);
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

// Test that, when the invoke animation finishes (when the active page is
// completely out of the view port), if Viz has already activated a new frame
// submitted by the new renderer, we skip `kWaitingForNewRendererToDraw`.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       RenderFrameActivatedDuringInvokeAnimation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  bool received_frame_while_waiting = false;
  GetAnimatorForTesting()->set_waited_for_renderer_new_frame(
      base::BindLambdaForTesting(
          [&]() { received_frame_while_waiting = true; }));

  TestNavigationManager back_to_red(web_contents(), RedURL());
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForResponse());

  // Manually set the new frame metadata before the DidCommit message and call
  // `OnRenderFrameMetadataChangedAfterActivation()` to simulate a frame
  // activation. Do this at the end of the "DidCommit" stack to simulate the
  // viz activates the first frame while the invoke animation is still playing.
  {
    RenderFrameHostImpl* red_rfh =
        GetAnimatorForTesting()->LastNavigationRequest()->GetRenderFrameHost();
    auto* new_widget_host = red_rfh->GetRenderWidgetHost();
    ASSERT_TRUE(new_widget_host);
    auto* new_view = new_widget_host->GetView();
    ASSERT_TRUE(new_view);
    cc::RenderFrameMetadata metadata;
    metadata.primary_main_frame_item_sequence_number =
        GetItemSequenceNumberForNavigation(back_to_red.GetNavigationHandle());
    GetAnimatorForTesting()->set_did_finish_navigation_callback(
        base::BindLambdaForTesting([&]() {
          new_widget_host->render_frame_metadata_provider()
              ->SetLastRenderFrameMetadataForTest(std::move(metadata));
          GetAnimatorForTesting()->OnRenderFrameMetadataChangedAfterActivation(
              base::TimeTicks());
        }));
  }

  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  destroyed.Run();
  ASSERT_FALSE(received_frame_while_waiting);
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

// E.g., google.com --back nav--> bank.com. Bank.com commits, but before the
// invoke animation has finished, bank.com's document redirects the user to
// bank.com/login.html.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       ClientRedirectWhileDisplayingInvokeAnimation) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  base::RunLoop did_finish_nav;
  GetAnimatorForTesting()->set_did_finish_navigation_callback(
      did_finish_nav.QuitClosure());
  GetAnimatorForTesting()->PauseAnimationAtDisplayingInvokeAnimation();

  TestNavigationManager back_to_red(web_contents(), RedURL());
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  did_finish_nav.Run();

  // Navigate to the blue page while the animator is still displaying the
  // invoke animation.
  GetAnimatorForTesting()->ExpectDisplayingInvokeAnimation();
  TestNavigationManager nav_to_blue(web_contents(), BlueURL());
  // Simulate a client redirect, from red's document.
  ASSERT_TRUE(ExecJs(web_contents(), "window.location.href = 'blue.html'"));
  ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  destroyed.Run();
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);
}

IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       ClientRedirectWhileWaitingForNewFrame) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  base::RunLoop did_finish_nav;
  GetAnimatorForTesting()->set_did_finish_navigation_callback(
      did_finish_nav.QuitClosure());
  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  bool cross_fade_displayed = false;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      base::BindLambdaForTesting([&]() { cross_fade_displayed = true; }));

  TestNavigationManager back_to_red(web_contents(), RedURL());
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_to_red.WaitForResponse());

  GetAnimatorForTesting()->set_intercept_render_frame_metadata_changed(true);
  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  did_finish_nav.Run();
  invoke_played.Run();
  GetAnimatorForTesting()->ExpectWaitingForNewFrame();

  TestNavigationManager nav_to_blue(web_contents(), BlueURL());
  // Simulate a client redirect, from red's document.
  ASSERT_TRUE(ExecJs(web_contents(), "window.location.href = 'blue.html'"));
  ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  destroyed.Run();
  ASSERT_FALSE(cross_fade_displayed);

  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);

  // [red, blue]. The green entry is pruned because of the client redirect.
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            BlueURL());
}

// Assert that navigating from a crashed page should have no impact on the
// animations.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       NavigatingFromACrashedPage) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // Crash the green page.
  RenderFrameHostWrapper crashed(web_contents()->GetPrimaryMainFrame());
  RenderProcessHostWatcher crashed_obs(
      crashed->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  crashed->GetProcess()->Shutdown(content::RESULT_CODE_KILLED);
  crashed_obs.Wait();
  ASSERT_TRUE(crashed.WaitUntilRenderFrameDeleted());
  // The crashed RFH is still owned by the RFHManager.
  ASSERT_FALSE(crashed.IsDestroyed());
  ASSERT_FALSE(crashed->IsRenderFrameLive());
  ASSERT_FALSE(crashed->GetView());

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  GetAnimatorForTesting()->set_navigating_from_a_crashed_page(true);

  TestFrameNavigationObserver back_to_red(web_contents());
  base::RunLoop cross_fade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  GetAnimationManager(web_contents())->OnGestureInvoked();
  cross_fade_displayed.Run();
  destroyed.Run();
  back_to_red.Wait();

  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

// Regression test for https://crbug.com/326516254: If the destination page is
// skipped for a back/forward navigation due to the lack of user activation, the
// animator should also skip that entry.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       SkipPageWithNoUserActivation) {
  auto& nav_controller = web_contents()->GetController();

  // [red&, green&, blue*]
  {
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
    observer.Wait();
    WaitForCopyableViewInWebContents(web_contents());
    ASSERT_EQ(nav_controller.GetEntryCount(), 3);
    ASSERT_EQ(nav_controller.GetCurrentEntryIndex(), 2);
  }

  // Mark green as skipped.
  nav_controller.GetEntryAtIndex(1)->set_should_skip_on_back_forward_ui(true);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k30ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  TestFrameNavigationObserver back_to_red(web_contents());
  base::RunLoop cross_fade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  GetAnimationManager(web_contents())->OnGestureInvoked();
  cross_fade_displayed.Run();
  destroyed.Run();
  back_to_red.Wait();

  // TODO(https://crbug.com/325329998): We should also test that the transition
  // is from blue to red via pixel comparison.

  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());
  ASSERT_EQ(nav_controller.GetEntryCount(), 3);
  ASSERT_EQ(nav_controller.GetCurrentEntryIndex(), 0);
}

namespace {

// Wait for the main frame to receive a UpdateUserActivationState from the
// renderer with the expected new state.
class BrowserUserActivationWaiter
    : public UpdateUserActivationStateInterceptor {
 public:
  BrowserUserActivationWaiter(
      RenderFrameHost* rfh,
      blink::mojom::UserActivationNotificationType expected_type)
      : UpdateUserActivationStateInterceptor(rfh),
        expected_type_(expected_type) {}
  ~BrowserUserActivationWaiter() override = default;

  // Blocks until the renderer sends the expected user activation via
  // `UpdateUserActivationState()`.
  void Wait() { run_loop_.Run(); }

  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) override {
    if (notification_type == expected_type_) {
      run_loop_.Quit();
    }
    UpdateUserActivationStateInterceptor::UpdateUserActivationState(
        update_type, notification_type);
  }

 private:
  const blink::mojom::UserActivationNotificationType expected_type_;
  base::RunLoop run_loop_;
};

// Inject a BeforeUnload handler into the main frame. Does NOT update the user
// activation.
void InjectBeforeUnloadForMainFrame(WebContentsImpl* web_contents,
                                    EvalJsOptions option) {
  static constexpr std::string_view kScript = R"(
    window.onbeforeunload = (event) => {
      // Recommended
      event.preventDefault();

      // Included for legacy support, e.g. Chrome/Edge < 119
      event.returnValue = true;
    };
  )";
  ASSERT_TRUE(ExecJs(web_contents, kScript, option));

  auto* main_frame =
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());

  if (option == EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE) {
    ASSERT_TRUE(
        main_frame->ShouldDispatchBeforeUnload(/*check_subframes_only=*/false));
    ASSERT_FALSE(main_frame->HasStickyUserActivation());
  } else {
    // Set the sticky user activation and let the bit propagate from renderer to
    // the browser.
    BrowserUserActivationWaiter wait_for_expected_user_activation(
        main_frame, blink::mojom::UserActivationNotificationType::kInteraction);
    SimulateMouseClick(web_contents, 0,
                       blink::WebPointerProperties::Button::kLeft);
    wait_for_expected_user_activation.Wait();
    ASSERT_TRUE(main_frame->ShouldDispatchBeforeUnload(
        /*check_subframes_only=*/false));
    ASSERT_TRUE(main_frame->HasStickyUserActivation());
  }
}

// Intercept the BeforeUnload dialog. Used to block the execution until the
// confirmation dialog shows up, and to interact with the dialog to either
// cancel or start the navigation.
class BeforeUnloadDialogObserver
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  explicit BeforeUnloadDialogObserver(RenderFrameHostImpl* main_frame)
      : main_frame_(main_frame), impl_(receiver().SwapImplForTesting(this)) {}
  ~BeforeUnloadDialogObserver() override = default;

  // `blink::mojom::LocalFrameHostInterceptorForTesting`:
  LocalFrameHost* GetForwardingInterface() override { return impl_; }
  void RunBeforeUnloadConfirm(
      bool is_reload,
      RunBeforeUnloadConfirmCallback callback) override {
    CHECK(!is_reload);
    ack_ = std::move(callback);
    run_loop_.Quit();
    // Reset immediately. `main_frame_` and `impl_` will be destroyed once
    // `ack_` is executed with "proceed".
    std::ignore = receiver().SwapImplForTesting(impl_);
    main_frame_ = nullptr;
    impl_ = nullptr;
  }

  void WaitForDialog() { run_loop_.Run(); }

  void RespondToDialogue(bool proceed) { std::move(ack_).Run(proceed); }

  [[nodiscard]] bool shown() const { return !main_frame_; }

 private:
  mojo::AssociatedReceiver<blink::mojom::LocalFrameHost>& receiver() {
    return main_frame_->local_frame_host_receiver_for_testing();
  }

  raw_ptr<RenderFrameHostImpl> main_frame_;
  raw_ptr<blink::mojom::LocalFrameHost> impl_;
  base::RunLoop run_loop_;
  RunBeforeUnloadConfirmCallback ack_;
};

}  // namespace

// Test the case where the renderer acks the BeforeUnload message without
// showing a prompt.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_Proceed_NoPrompt) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadForMainFrame(web_contents(),
                                 EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  base::RunLoop did_finish_nav;
  GetAnimatorForTesting()->set_did_finish_navigation_callback(
      did_finish_nav.QuitClosure());
  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop cross_fade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.QuitClosure());
  bool cancel_displayed = false;
  GetAnimatorForTesting()->set_on_cancel_animation_displayed(
      base::BindLambdaForTesting([&]() { cancel_displayed = true; }));

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimationManager(web_contents())->OnGestureInvoked();

  invoke_played.Run();
  cross_fade_displayed.Run();
  did_finish_nav.Run();
  destroyed.Run();
  back_to_red.Wait();
  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());

  ASSERT_FALSE(dialog_observer.shown());
  ASSERT_FALSE(cancel_displayed);
}

// Test the case where the renderer shows a prompt for the BeforeUnload message,
// and the user decides to proceed.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_Proceed_WithPrompt) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);
  InjectBeforeUnloadForMainFrame(web_contents(),
                                 EvalJsOptions::EXECUTE_SCRIPT_DEFAULT_OPTIONS);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  base::RunLoop did_finish_nav;
  GetAnimatorForTesting()->set_did_finish_navigation_callback(
      did_finish_nav.QuitClosure());
  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop cancel_displayed;
  GetAnimatorForTesting()->set_on_cancel_animation_displayed(
      cancel_displayed.QuitClosure());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimationManager(web_contents())->OnGestureInvoked();

  cancel_displayed.Run();
  dialog_observer.WaitForDialog();
  GetAnimatorForTesting()->ExpectWaitingForBeforeUnloadResponse();
  dialog_observer.RespondToDialogue(/*proceed=*/true);

  invoke_played.Run();
  did_finish_nav.Run();
  destroyed.Run();
  back_to_red.Wait();
  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());

  ASSERT_TRUE(dialog_observer.shown());
}

// Test the case where the user cancels the navigation via the prompt, after
// the cancel animation finishes.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_Cancel_AfterCancelAnimationFinishes) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadForMainFrame(web_contents(),
                                 EvalJsOptions::EXECUTE_SCRIPT_DEFAULT_OPTIONS);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  bool invoke_played = false;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      base::BindLambdaForTesting([&]() { invoke_played = true; }));
  base::RunLoop cancel_displayed;
  GetAnimatorForTesting()->set_on_cancel_animation_displayed(
      cancel_displayed.QuitClosure());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimationManager(web_contents())->OnGestureInvoked();

  cancel_displayed.Run();
  dialog_observer.WaitForDialog();
  GetAnimatorForTesting()->ExpectWaitingForBeforeUnloadResponse();
  dialog_observer.RespondToDialogue(/*proceed=*/false);

  destroyed.Run();
  ASSERT_FALSE(back_to_red.last_navigation_succeeded());

  ASSERT_FALSE(invoke_played);
  ASSERT_TRUE(dialog_observer.shown());
}

// Test the case where the user cancels the navigation via the prompt, before
// the cancel animation finishes.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_Cancel_BeforeCancelAnimationFinishes) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadForMainFrame(web_contents(),
                                 EvalJsOptions::EXECUTE_SCRIPT_DEFAULT_OPTIONS);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  bool invoke_played = false;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      base::BindLambdaForTesting([&]() { invoke_played = true; }));
  base::RunLoop cancel_displayed;
  GetAnimatorForTesting()->set_on_cancel_animation_displayed(
      cancel_displayed.QuitClosure());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimatorForTesting()->PauseAnimationAtDisplayingCancelAnimation();
  GetAnimationManager(web_contents())->OnGestureInvoked();

  dialog_observer.WaitForDialog();
  GetAnimatorForTesting()->ExpectDisplayingCancelAnimation();
  dialog_observer.RespondToDialogue(/*proceed=*/false);
  GetAnimatorForTesting()->UnpauseAnimation();

  cancel_displayed.Run();
  destroyed.Run();
  ASSERT_FALSE(back_to_red.last_navigation_succeeded());

  ASSERT_FALSE(invoke_played);
  ASSERT_TRUE(dialog_observer.shown());
}

// Test that when the user has decided not leave the current page by interacting
// with the prompt and the cancel animation is still playing, another navigation
// commits in the main frame. We should destroy the animator when the other
// navigation commits.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_RequestCancelledBeforeStart) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadForMainFrame(web_contents(),
                                 EvalJsOptions::EXECUTE_SCRIPT_DEFAULT_OPTIONS);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  bool invoke_played = false;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      base::BindLambdaForTesting([&]() { invoke_played = true; }));
  bool cancel_finished_playing = false;
  GetAnimatorForTesting()->set_on_cancel_animation_displayed(
      base::BindLambdaForTesting([&]() { cancel_finished_playing = true; }));

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  TestFrameNavigationObserver back_to_red(web_contents());
  GetAnimatorForTesting()->set_duration_between_frames(base::Microseconds(1));
  GetAnimatorForTesting()->PauseAnimationAtDisplayingCancelAnimation();
  GetAnimationManager(web_contents())->OnGestureInvoked();

  dialog_observer.WaitForDialog();
  GetAnimatorForTesting()->ExpectDisplayingCancelAnimation();
  // Expectation the animator will be destroyed while playing the cancel
  // animation.
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  dialog_observer.RespondToDialogue(/*proceed=*/false);
  GetAnimatorForTesting()->UnpauseAnimation();

  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  destroyed.Run();

  ASSERT_FALSE(invoke_played);
  ASSERT_FALSE(cancel_finished_playing);
  ASSERT_TRUE(dialog_observer.shown());

  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 3);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 2);
}

namespace {
class FailBeginNavigationImpl : public ContentBrowserTestContentBrowserClient {
 public:
  FailBeginNavigationImpl() = default;
  ~FailBeginNavigationImpl() override = default;

  // `ContentBrowserTestContentBrowserClient`:
  bool ShouldOverrideUrlLoading(int frame_tree_node_id,
                                bool browser_initiated,
                                const GURL& gurl,
                                const std::string& request_method,
                                bool has_user_gesture,
                                bool is_redirect,
                                bool is_outermost_main_frame,
                                bool is_prerendering,
                                ui::PageTransition transition,
                                bool* ignore_navigation) final {
    // See `NavigationRequest::BeginNavigationImpl()`.
    *ignore_navigation = true;
    return true;
  }
};
}  // namespace

// Test that the animator is behaving correctly, even after the renderer acks
// the BeforeUnload message to proceed (begin) the navigation, but
// `BeginNavigationImpl()` hits an early out so we never each
// `DidStartNavigation()`.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       BeforeUnload_BeginNavigationImplFails) {
  FailBeginNavigationImpl fail_begin_navigation_client;

  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  InjectBeforeUnloadForMainFrame(web_contents(),
                                 EvalJsOptions::EXECUTE_SCRIPT_DEFAULT_OPTIONS);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  base::RunLoop cancel_displayed;
  GetAnimatorForTesting()->set_on_cancel_animation_displayed(
      cancel_displayed.QuitClosure());

  BeforeUnloadDialogObserver dialog_observer(
      web_contents()->GetPrimaryMainFrame());
  GetAnimationManager(web_contents())->OnGestureInvoked();

  cancel_displayed.Run();
  dialog_observer.WaitForDialog();
  GetAnimatorForTesting()->ExpectWaitingForBeforeUnloadResponse();
  dialog_observer.RespondToDialogue(/*proceed=*/true);

  destroyed.Run();
  ASSERT_TRUE(dialog_observer.shown());

  // Still on the green page.
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            GreenURL());
}

// Testing that, on the back nav from green.html to red.html, red.html redirects
// to blue.html. while the cross-fading animation is playing from the red.html's
// screenshot to the live page. We should abort the cross-fade animation when
// the redirect to blue.html commits.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       ClientRedirect_AnimatorDestroyedDuringCrossFade) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  GURL client_redirect =
      embedded_test_server()->GetURL("/red_redirect_to_blue.html#redirect");

  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  web_contents()->GetController().GetEntryAtIndex(0)->SetURL(client_redirect);

  TestNavigationManager back_nav_to_red(web_contents(), client_redirect);
  TestNavigationManager nav_to_blue(web_contents(), BlueURL());

  GetAnimatorForTesting()->PauseAnimationAtDisplayingCrossFadeAnimation();
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();

  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());
  // Force a call of `OnRenderFrameMetadataChangedAfterActivation()` when the
  // navigation back to red is committed. This makes sure that the animation
  // manager is displaying the cross-fade animation while the redirec to blue
  // is happening.
  {
    RenderFrameHostImpl* red_rfh =
        GetAnimatorForTesting()->LastNavigationRequest()->GetRenderFrameHost();
    auto* new_widget_host = red_rfh->GetRenderWidgetHost();
    ASSERT_TRUE(new_widget_host);
    auto* new_view = new_widget_host->GetView();
    ASSERT_TRUE(new_view);
    cc::RenderFrameMetadata metadata;
    metadata.primary_main_frame_item_sequence_number =
        GetItemSequenceNumberForNavigation(
            back_nav_to_red.GetNavigationHandle());
    GetAnimatorForTesting()->set_did_finish_navigation_callback(
        base::BindLambdaForTesting([&]() {
          new_widget_host->render_frame_metadata_provider()
              ->SetLastRenderFrameMetadataForTest(std::move(metadata));
          GetAnimatorForTesting()->OnRenderFrameMetadataChangedAfterActivation(
              base::TimeTicks());
        }));
  }

  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(back_nav_to_red.was_successful());
  invoke_played.Run();
  GetAnimatorForTesting()->ExpectWaitingForDisplayingCrossFadeAnimation();

  ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  destroyed.Run();

  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            BlueURL());
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(1)->GetURL(),
            GreenURL());
}

// Test that input isn't dispatched to the renderer while the transition
// animation is in progress.
// TODO(bokan): Re-enable once crbug.com/344620149 is fixed.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       DISABLED_SuppressRendererInputDuringTransition) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // Start a back transition gesture.
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  // Once the gesture's invoked, block the response so we're waiting with the
  // transition active.
  TestNavigationManager back_nav_to_green(web_contents(), RedURL());
  GetAnimationManager(web_contents())->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_green.WaitForResponse());

  // Simulate various kinds of user input, these events should not be dispatched
  // to the renderer.
  {
    InputMsgWatcher input_watcher(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kUndefined);
    SimulateGestureScrollSequence(web_contents(), gfx::Point(100, 100),
                                  gfx::Vector2dF(0, 50));
    RunUntilInputProcessed(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
    EXPECT_EQ(input_watcher.last_sent_event_type(),
              blink::WebInputEvent::Type::kUndefined);

    SimulateTapDownAt(web_contents(), gfx::Point(100, 100));
    SimulateTapAt(web_contents(), gfx::Point(100, 100));
    RunUntilInputProcessed(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());

    EXPECT_EQ(input_watcher.last_sent_event_type(),
              blink::WebInputEvent::Type::kUndefined);

    SimulateMouseClick(web_contents(),
                       blink::WebInputEvent::Modifiers::kNoModifiers,
                       blink::WebMouseEvent::Button::kLeft);

    RunUntilInputProcessed(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
    EXPECT_EQ(input_watcher.last_sent_event_type(),
              blink::WebInputEvent::Type::kUndefined);
  }

  // Unblock the navigation and wait until the transition is completed.
  ASSERT_TRUE(back_nav_to_green.WaitForNavigationFinished());
  ASSERT_TRUE(back_nav_to_green.was_successful());
  invoke_played.Run();
  destroyed.Run();

  // Ensure input is now successfully dispatched.
  {
    InputMsgWatcher input_watcher(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kUndefined);
    SimulateTapDownAt(web_contents(), gfx::Point(100, 100));
    SimulateTapAt(web_contents(), gfx::Point(100, 100));
    RunUntilInputProcessed(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
    EXPECT_EQ(input_watcher.last_sent_event_type(),
              blink::WebInputEvent::Type::kGestureTap);
  }
}

// Regression test for https://crbug.com/339501357: If the animator is destroyed
// in the middle of a gesture, the history navigation should still proceed.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       AnimatorDestroyedMidGesture) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  // Start a navigation and wait until the request has been sent
  TestNavigationManager nav_to_blue(web_contents(), BlueURL());
  web_contents()->GetController().LoadURL(
      BlueURL(), Referrer{},
      ui::PageTransitionFromInt(
          ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR |
          ui::PageTransition::PAGE_TRANSITION_TYPED),
      std::string{});
  ASSERT_TRUE(nav_to_blue.WaitForRequestStart());

  // Start a swipe gesture
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k30ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  // When the navigation above commits the animator should be destroyed with an
  // abort
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  destroyed.Run();

  auto* manager = static_cast<BackForwardTransitionAnimationManagerAndroid*>(
      web_contents()->GetBackForwardTransitionAnimationManager());

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  manager->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());
  ASSERT_TRUE(back_nav_to_red.was_committed());
}

// Regression test for https://crbug.com/344761329: If the
// WebContentsViewAndroid's native view is detached from the root window, we
// should abort the transition.
IN_PROC_BROWSER_TEST_F(BackForwardTransitionAnimationManagerBrowserTest,
                       AnimatorDestroyedWhenViewAndroidDetachedFromWindow) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k30ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  // Pause at the beginning of the invoke animation but wait for the navigation
  // to finish, so we can guarantee to have subscribed to the new
  // RenderWidgetHost.
  GetAnimatorForTesting()->PauseAnimationAtDisplayingInvokeAnimation();
  TestNavigationManager back_nav_to_red(web_contents(), RedURL());
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForNavigationFinished());

  web_contents()->GetWebContentsAndroid()->SetTopLevelNativeWindow(
      /*env=*/nullptr,
      /*jwindow_android=*/base::android::JavaParamRef<jobject>(nullptr));
  destroyed.Run();
}

class BackForwardTransitionAnimationManagerBrowserTestWithProgressBar
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BackForwardTransitionAnimationManagerBrowserTest::SetUpOnMainThread();
    web_contents()
        ->GetNativeView()
        ->GetWindowAndroid()
        ->set_progress_bar_config_for_testing(kConfig);
  }

  void ValidateNoProgressBar() {
    const auto& screenshot_layer = GetScreenshotLayer();
    EXPECT_EQ(screenshot_layer->children().size(), 1u);
  }

  const scoped_refptr<cc::slim::Layer> GetProgressBar() {
    const auto& screenshot_layer = GetScreenshotLayer();
    EXPECT_EQ(screenshot_layer->children().size(), 2u);
    return screenshot_layer->children().at(1);
  }

 protected:
  static constexpr ui::ProgressBarConfig kConfig = {
      .background_color = SkColors::kWhite,
      .height_physical = 10,
      .color = SkColors::kBlue,
      .hairline_color = SkColors::kWhite};
};

// Tests that the progress bar is drawn at the correct position during the
// invoke phase.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestWithProgressBar,
    ProgressBar) {
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k30ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);
  ValidateNoProgressBar();

  GetAnimationManager(web_contents())->OnGestureInvoked();
  {
    // Progress bar should be displayed when invoke animation starts.
    base::test::TestFuture<void> on_animate;
    GetAnimatorForTesting()->set_next_on_animate_callback(
        on_animate.GetCallback());
    ASSERT_TRUE(on_animate.Wait())
        << "Timed out waiting for invoke animation to start";
    GetAnimatorForTesting()->ExpectDisplayingInvokeAnimation();
    const auto& progress_layer = GetProgressBar();
    const int viewport_width = GetViewportSize().width();
    EXPECT_EQ(progress_layer->bounds(),
              gfx::Size(viewport_width, kConfig.height_physical));
  }

  {
    base::test::TestFuture<void> invoke_played;
    GetAnimatorForTesting()->set_on_invoke_animation_displayed(
        invoke_played.GetCallback());
    ASSERT_TRUE(invoke_played.Wait())
        << "Timed out waiting for invoke animation to finish";

    base::test::TestFuture<void> on_animate;
    GetAnimatorForTesting()->set_next_on_animate_callback(
        on_animate.GetCallback());
    ASSERT_TRUE(on_animate.Wait())
        << "Timed out waiting for animation after invoke to start";

    // Progress bar should be removed.
    ValidateNoProgressBar();
  }

  base::test::TestFuture<void> on_destroyed;
  GetAnimatorForTesting()->set_next_on_animate_callback(
      on_destroyed.GetCallback());
  ASSERT_TRUE(on_destroyed.Wait())
      << "Timed out waiting for animator to be destroyed";
}

class BackForwardTransitionAnimationManagerBrowserTestWithNavigationQueueing
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  BackForwardTransitionAnimationManagerBrowserTestWithNavigationQueueing() =
      default;
  ~BackForwardTransitionAnimationManagerBrowserTestWithNavigationQueueing()
      override = default;

  void SetUp() override {
    BackForwardTransitionAnimationManagerBrowserTest::SetUp();

    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kQueueNavigationsWhileWaitingForCommit,
         {{"queueing_level", "full"}}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardTransitionAnimationManagerBrowserTest::SetUpCommandLine(
        command_line);
    // Force --site-per-process because this test is testing races with
    // committing a navigation in a speculative `RenderFrameHost`.
    command_line->AppendSwitch(switches::kSitePerProcess);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Assert that once the gesture navigation has sent the commit message to the
// renderer, the animation will not be cancelled.
//
// TODO(https://crbug.com/326256165): Re-enable this in a follow up.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestWithNavigationQueueing,
    DISABLED_QueuedNavigationNoCancel) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);

  // We haven't started the navigation at this point.
  HistoryBackNavAndAssertAnimatedTransition(expected);

  TestNavigationManager back_nav_to_red(web_contents(), RedURL());

  // Set the interceptor, so we start the navigation to blue when the
  // DidCommit message to red has just arrived at the browser.
  CommitMessageDelayer delay_nav_to_red(
      web_contents(), RedURL(),
      base::BindOnce(
          [](WebContentsImpl* web_contents, const GURL& blue_url,
             RenderFrameHost* rfh) {
            web_contents->GetController().LoadURL(
                blue_url, Referrer{},
                ui::PageTransitionFromInt(
                    ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                    ui::PageTransition::PAGE_TRANSITION_TYPED),
                std::string{});
          },
          web_contents(), BlueURL()));

  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  // The user has lifted the finger - signaling the start of the navigation.
  auto* animation_manager = GetAnimationManager(web_contents());
  animation_manager->OnGestureInvoked();
  ASSERT_TRUE(back_nav_to_red.WaitForResponse());

  // Wait for the navigation to the blue page has started.
  TestNavigationManager nav_to_blue(web_contents(), BlueURL());
  back_nav_to_red.ResumeNavigation();

  // Wait for the DidCommit message to red is intercepted, and then the
  // navigation to blue has started.
  delay_nav_to_red.Wait();
  // Pause the navigation to the blue page so we can let the committing red
  // navigation and its animations to finish.
  ASSERT_TRUE(nav_to_blue.WaitForRequestStart());

  // The start of navigation to the blue page means the history nav to the red
  // page has committed. Since the history nav to the red page has committed,
  // the animation manager must have brought the red page to the center of the
  // viewport.
  ASSERT_TRUE(back_nav_to_red.was_successful());
  invoke_played.Run();
  destroyed.Run();
  ExpectedLayerTransforms(web_contents(), kActivePageAtOrigin);

  // Wait for the navigation to the blue have finished.
  ASSERT_TRUE(nav_to_blue.WaitForNavigationFinished());
  ASSERT_TRUE(nav_to_blue.was_successful());

  // [red, blue]. The green NavigationEntry is pruned because we performed a
  // forward navigation from red to blue.
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntryIndex(), 1);
  ASSERT_EQ(web_contents()->GetController().GetLastCommittedEntry()->GetURL(),
            BlueURL());
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            RedURL());
}

class BackForwardTransitionAnimationManagerBrowserTestDeviceScalingFactor
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  BackForwardTransitionAnimationManagerBrowserTestDeviceScalingFactor() =
      default;
  ~BackForwardTransitionAnimationManagerBrowserTestDeviceScalingFactor()
      override = default;

  void SetUp() override {
    BackForwardTransitionAnimationManagerBrowserTest::SetUp();
    EnablePixelOutput(/*force_device_scale_factor=*/1.333f);
  }
};

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestDeviceScalingFactor,
    Invoke) {
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k30ViewportWidth);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::k90ViewportWidth);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::k30ViewportWidth);
  expected.push_back(GestureType::k60ViewportWidth);
  expected.push_back(GestureType::k90ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  TestFrameNavigationObserver back_to_red(web_contents());
  base::RunLoop cross_fade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      cross_fade_displayed.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());
  GetAnimationManager(web_contents())->OnGestureInvoked();
  cross_fade_displayed.Run();
  destroyed.Run();
  back_to_red.Wait();

  ASSERT_EQ(back_to_red.last_committed_url(), RedURL());
  ASSERT_FALSE(web_contents()->GetController().GetActiveEntry()->GetUserData(
      NavigationEntryScreenshot::kUserDataKey));
}

namespace {

class BackForwardTransitionAnimationManagerWithRedirectBrowserTest
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  BackForwardTransitionAnimationManagerWithRedirectBrowserTest() = default;
  ~BackForwardTransitionAnimationManagerWithRedirectBrowserTest() override =
      default;

  void SetUpOnMainThread() override {
    SetupCrossSiteRedirector(embedded_test_server());
    BackForwardTransitionAnimationManagerBrowserTest::SetUpOnMainThread();
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerWithRedirectBrowserTest,
    AbortedOnCrossOriginRedirect) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  bool invoke_played = false;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      base::BindLambdaForTesting([&]() { invoke_played = true; }));
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  std::string different_host("b.com");
  GURL redirect = embedded_test_server()->GetURL(
      "/cross-site/" + different_host + "/empty.html");
  GURL expected_url =
      embedded_test_server()->GetURL(different_host, "/empty.html");

  // [red&, green*]
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  web_contents()->GetController().GetEntryAtIndex(0)->SetURL(redirect);

  TestNavigationManager redirect_nav(web_contents(), redirect);

  GetAnimatorForTesting()->SetFinishedStateToAnimationAborted();
  GetAnimationManager(web_contents())->OnGestureInvoked();

  ASSERT_TRUE(redirect_nav.WaitForNavigationFinished());
  destroyed.Run();
  ASSERT_FALSE(invoke_played);

  // [empty.html*, green&]
  ASSERT_EQ(web_contents()->GetController().GetEntryCount(), 2);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(0)->GetURL(),
            expected_url);
  ASSERT_EQ(web_contents()->GetController().GetEntryAtIndex(1)->GetURL(),
            GreenURL());
}

// Assert that the navigation back to a site with an opaque origin is not
// considered as redirect. Such sites can be "chrome://newtabpage", "data:" or
// "file://".
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerWithRedirectBrowserTest,
    OpaqueOriginsAreNotRedirects) {
  DisableBackForwardCacheForTesting(
      web_contents(),
      BackForwardCache::DisableForTestingReason::TEST_REQUIRES_NO_CACHING);

  constexpr char kGreenDataURL[] = R"(
    data:text/html,<body style="background-color:green"></body>
  )";

  ASSERT_TRUE(NavigateToURL(web_contents(), GURL(kGreenDataURL)));
  WaitForCopyableViewInWebContents(web_contents());
  ASSERT_TRUE(NavigateToURL(web_contents(), BlueURL()));
  WaitForCopyableViewInWebContents(web_contents());

  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop invoke_played;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_played.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  TestNavigationManager back_nav_to_data_url(web_contents(),
                                             GURL(kGreenDataURL));

  GetAnimationManager(web_contents())->OnGestureInvoked();

  ASSERT_TRUE(back_nav_to_data_url.WaitForNavigationFinished());
  invoke_played.Run();
  destroyed.Run();
}

namespace {

class BackForwardTransitionAnimationManagerBrowserTestSameDocument
    : public BackForwardTransitionAnimationManagerBrowserTest {
 public:
  BackForwardTransitionAnimationManagerBrowserTestSameDocument() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kIncrementLocalSurfaceIdForMainframeSameDocNavigation,
         {}}};
    scoped_feature_list_for_same_doc_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{});
  }
  ~BackForwardTransitionAnimationManagerBrowserTestSameDocument() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Disable the vertical scroll bar, otherwise they might show up on the
    // screenshot, making the test flaky.
    command_line->AppendSwitch(switches::kHideScrollbars);
    BackForwardTransitionAnimationManagerBrowserTest::SetUpCommandLine(
        command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Load the red portion of the page.
    ASSERT_TRUE(NavigateToURL(web_contents(), embedded_test_server()->GetURL(
                                                  "/changing_color.html")));
    WaitForCopyableViewInWebContents(web_contents());

    auto* manager =
        BrowserContextImpl::From(web_contents()->GetBrowserContext())
            ->GetNavigationEntryScreenshotManager();
    ASSERT_TRUE(manager);
    ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
    ASSERT_TRUE(web_contents()->GetRenderWidgetHostView());

    // Limit three screenshots.
    manager->SetMemoryBudgetForTesting(4 * GetViewportSize().Area64() * 3);

    auto& controller = web_contents()->GetController();
    // Navigate to the green portion of the page.
    const int num_request_before_nav =
        NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting();
    const int entries_count_before_nav = controller.GetEntryCount();
    {
      ScopedScreenshotCapturedObserverForTesting observer(
          controller.GetLastCommittedEntryIndex());
      ASSERT_TRUE(NavigateToURL(
          web_contents(),
          embedded_test_server()->GetURL("/changing_color.html#green")));
      observer.Wait();
    }
    ASSERT_EQ(controller.GetEntryCount(), entries_count_before_nav + 1);
    ASSERT_EQ(
        NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting(),
        num_request_before_nav + 1);

    auto* animation_manager = GetAnimationManager(web_contents());
    animation_manager->set_animator_factory_for_testing(
        std::make_unique<FactoryForTesting>());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_for_same_doc_;
};

}  // namespace

// Basic test for the animated transition on same-doc navigations. The
// transition is from a green portion of a page to a red portion of the same
// page.
IN_PROC_BROWSER_TEST_F(
    BackForwardTransitionAnimationManagerBrowserTestSameDocument,
    SmokeTest) {
  std::vector<GestureType> expected;
  expected.push_back(GestureType::kStart);
  expected.push_back(GestureType::k60ViewportWidth);
  HistoryBackNavAndAssertAnimatedTransition(expected);

  base::RunLoop invoke_displayed;
  GetAnimatorForTesting()->set_on_invoke_animation_displayed(
      invoke_displayed.QuitClosure());
  base::RunLoop crossfade_displayed;
  GetAnimatorForTesting()->set_on_cross_fade_animation_displayed(
      crossfade_displayed.QuitClosure());
  base::RunLoop destroyed;
  GetAnimatorForTesting()->set_on_impl_destroyed(destroyed.QuitClosure());

  TestNavigationManager back_to_red(
      web_contents(), embedded_test_server()->GetURL("/changing_color.html"));
  GetAnimationManager(web_contents())->OnGestureInvoked();

  ASSERT_TRUE(back_to_red.WaitForNavigationFinished());
  invoke_displayed.Run();
  crossfade_displayed.Run();
  destroyed.Run();
}

}  // namespace content

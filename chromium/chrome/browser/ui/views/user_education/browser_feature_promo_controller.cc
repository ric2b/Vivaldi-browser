// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

BrowserFeaturePromoController::BrowserFeaturePromoController(
    BrowserView* browser_view,
    feature_engagement::Tracker* feature_engagement_tracker,
    user_education::FeaturePromoRegistry* registry,
    user_education::HelpBubbleFactoryRegistry* help_bubble_registry,
    user_education::FeaturePromoStorageService* storage_service,
    user_education::FeaturePromoSessionPolicy* session_policy,
    user_education::TutorialService* tutorial_service,
    user_education::ProductMessagingController* messaging_controller)
    : FeaturePromoControllerCommon(feature_engagement_tracker,
                                   registry,
                                   help_bubble_registry,
                                   storage_service,
                                   session_policy,
                                   tutorial_service,
                                   messaging_controller),
      browser_view_(browser_view) {}

BrowserFeaturePromoController::~BrowserFeaturePromoController() = default;

// static
BrowserFeaturePromoController* BrowserFeaturePromoController::GetForView(
    views::View* view) {
  if (!view) {
    return nullptr;
  }
  views::Widget* widget = view->GetWidget();
  if (!widget) {
    return nullptr;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      widget->GetPrimaryWindowWidget()->GetNativeWindow());
  if (!browser_view) {
    return nullptr;
  }

  return browser_view->GetFeaturePromoController();
}

ui::ElementContext BrowserFeaturePromoController::GetAnchorContext() const {
  return views::ElementTrackerViews::GetContextForView(browser_view_);
}

bool BrowserFeaturePromoController::CanShowPromoForElement(
    ui::TrackedElement* anchor_element) const {
  // Trying to show an IPH while the browser is closing can cause problems;
  // see crbug.com/346461762 for an example.
  if (browser_view_->browser()->IsBrowserClosing()) {
    return false;
  }

  auto* const profile = browser_view_->GetProfile();

  // Turn off IPH while a required privacy interstitial is visible or pending.
  // TODO(dfried): with Desktop User Education 2.0, filtering of IPH may need to
  // be more nuanced; also a contention scheme between required popups may be
  // required. See go/desktop-user-education-2.0 for details.
  auto* const privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile);
  if (privacy_sandbox_service &&
      privacy_sandbox_service->GetRequiredPromptType() !=
          PrivacySandboxService::PromptType::kNone) {
    return false;
  }

  // Turn off IPH while a required search engine choice dialog is visible or
  // pending.
  if (search_engines::IsChoiceScreenFlagEnabled(
          search_engines::ChoicePromo::kDialog)) {
    Browser& browser = *browser_view_->browser();
    SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
        SearchEngineChoiceDialogServiceFactory::GetForProfile(
            browser.profile());
    if (search_engine_choice_dialog_service &&
        search_engine_choice_dialog_service->HasPendingDialog(browser)) {
      return false;
    }
  }

  // Don't show IPH if the toolbar is collapsed in Responsive Mode/the overflow
  // button is visible.
  //
  // TODO(dfried): make this more specific for certain types of promos. For
  // example, a toast IPH anchored to an element that's actually visible should
  // be fine, but we might want to avoid Tutorial and Custom Action IPH even if
  // the initial anchor is present.
  if (base::FeatureList::IsEnabled(features::kResponsiveToolbar)) {
    if (const auto* const controller =
            browser_view_->toolbar()->toolbar_controller()) {
      if (controller->InOverflowMode()) {
        return false;
      }
    }
  }

  // Don't show IPH if the anchor view is in an inactive window.
  auto* const anchor_view = anchor_element->AsA<views::TrackedElementViews>();
  auto* const anchor_widget = anchor_view ? anchor_view->view()->GetWidget()
                                          : browser_view_->GetWidget();
  if (!anchor_widget) {
    return false;
  }

  if (!active_window_check_blocked() && !anchor_widget->ShouldPaintAsActive()) {
    return false;
  }

  return true;
}

const ui::AcceleratorProvider*
BrowserFeaturePromoController::GetAcceleratorProvider() const {
  return browser_view_;
}

std::u16string BrowserFeaturePromoController::GetTutorialScreenReaderHint()
    const {
  ui::Accelerator accelerator;
  std::u16string accelerator_text;
#if BUILDFLAG(IS_CHROMEOS)
  // IDC_FOCUS_NEXT_PANE still reports as F6 on ChromeOS, but many ChromeOS
  // devices do not have function keys. Therefore, instead prompt the other
  // accelerator that does the same thing.
  static const auto kAccelerator = IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY;
#else
  static const auto kAccelerator = IDC_FOCUS_NEXT_PANE;
#endif
  if (browser_view_->GetAccelerator(kAccelerator, &accelerator)) {
    accelerator_text = accelerator.GetShortcutText();
  } else {
    // TODO(crbug.com/40903127): GetAccelerator appears to be failing
    // sporadically on Windows, for unknown reasons. Since we can't have this
    // code crashing in release, it's being returned to the original NOTREACHED
    // before everything was changed to CHECKs. This bug will continue to be
    // researched for a more correct fix.
    accelerator_text = u"F6";
    NOTREACHED_IN_MIGRATION();
  }
  return l10n_util::GetStringFUTF16(IDS_FOCUS_HELP_BUBBLE_TUTORIAL_DESCRIPTION,
                                    accelerator.GetShortcutText());
}

std::u16string
BrowserFeaturePromoController::GetFocusHelpBubbleScreenReaderHint(
    user_education::FeaturePromoSpecification::PromoType promo_type,
    ui::TrackedElement* anchor_element,
    bool is_critical_promo) const {
  return GetFocusHelpBubbleScreenReaderHintCommon(
      promo_type, browser_view_, anchor_element, is_critical_promo);
}

std::u16string GetFocusHelpBubbleScreenReaderHintCommon(
    user_education::FeaturePromoSpecification::PromoType promo_type,
    const ui::AcceleratorProvider* accelerator_provider,
    ui::TrackedElement* anchor_element,
    bool is_critical_promo) {
  // No message is required as this is a background bubble with a
  // screen reader-specific prompt and will dismiss itself.
  if (promo_type ==
      user_education::FeaturePromoSpecification::PromoType::kToast) {
    return std::u16string();
  }

  ui::Accelerator accelerator;
  std::u16string accelerator_text;
  CHECK(accelerator_provider->GetAcceleratorForCommandId(IDC_FOCUS_NEXT_PANE,
                                                         &accelerator));
  accelerator_text = accelerator.GetShortcutText();

  // Present the user with the full help bubble navigation shortcut.
  auto* const anchor_view = anchor_element->AsA<views::TrackedElementViews>();
  if (promo_type ==
          user_education::FeaturePromoSpecification::PromoType::kTutorial ||
      (anchor_view &&
       (anchor_view->view()
            ->GetViewAccessibility()
            .IsAccessibilityFocusable() ||
        views::IsViewClass<views::AccessiblePaneView>(anchor_view->view())))) {
    return l10n_util::GetStringFUTF16(IDS_FOCUS_HELP_BUBBLE_TOGGLE_DESCRIPTION,
                                      accelerator_text);
  }

  // If the bubble starts focused and focus cannot traverse to the anchor view,
  // do not use a promo.
  if (is_critical_promo) {
    return std::u16string();
  }

  // Present the user with an abridged help bubble navigation shortcut.
  return l10n_util::GetStringFUTF16(IDS_FOCUS_HELP_BUBBLE_DESCRIPTION,
                                    accelerator_text);
}

std::u16string BrowserFeaturePromoController::GetBodyIconAltText() const {
  return l10n_util::GetStringUTF16(IDS_CHROME_TIP);
}

const base::Feature*
BrowserFeaturePromoController::GetScreenReaderPromptPromoFeature() const {
  return &feature_engagement::kIPHFocusHelpBubbleScreenReaderPromoFeature;
}

const char* BrowserFeaturePromoController::GetScreenReaderPromptPromoEventName()
    const {
  return feature_engagement::events::kFocusHelpBubbleAcceleratorPromoRead;
}

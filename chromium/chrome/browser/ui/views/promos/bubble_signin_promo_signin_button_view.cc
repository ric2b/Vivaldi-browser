// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/promos/bubble_signin_promo_signin_button_view.h"

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace {

constexpr base::TimeDelta kDoubleClickSignInPreventionDelay =
    base::Seconds(0.5);

}  // namespace

BubbleSignInPromoSignInButtonView::BubbleSignInPromoSignInButtonView(
    views::Button::PressedCallback callback,
    signin_metrics::AccessPoint access_point,
    ui::ButtonStyle button_style,
    std::u16string button_text)
    : account_(std::nullopt) {
  views::MdTextButton* text_button = nullptr;

  views::Builder<BubbleSignInPromoSignInButtonView>(this)
      .SetUseDefaultFillLayout(true)
      .AddChild(
          // Regular MD text button when there is no account.
          views::Builder<views::MdTextButton>()
              .SetText(button_text)
              .SetStyle(button_style)
              .CopyAddressTo(&text_button))
      .BuildChildren();

  // Add the callback to the button with a delay if it is an autofill sign in
  // promo.
  AddOrDelayCallbackForSignInButton(access_point, text_button, callback);

  SetProperty(views::kElementIdentifierKey, kPromoSignInButton);
}

BubbleSignInPromoSignInButtonView::BubbleSignInPromoSignInButtonView(
    const AccountInfo& account,
    const gfx::Image& account_icon,
    views::Button::PressedCallback callback,
    signin_metrics::AccessPoint access_point,
    std::u16string button_text,
    std::u16string button_accessibility_text)
    : account_(account) {
  bool is_autofill_promo =
      access_point == signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE;

  DCHECK(!account_icon.IsEmpty());
  auto card_title = base::UTF8ToUTF16(account.full_name);

  const views::BoxLayout::Orientation orientation =
      is_autofill_promo ? views::BoxLayout::Orientation::kVertical
                        : views::BoxLayout::Orientation::kHorizontal;

  std::unique_ptr<views::BoxLayout> button_layout =
      std::make_unique<views::BoxLayout>(orientation, gfx::Insets(), 16);

  std::unique_ptr<HoverButton> hover_button = std::make_unique<HoverButton>(
      views::Button::PressedCallback(),
      std::make_unique<BadgedProfilePhoto>(
          is_autofill_promo ? BadgedProfilePhoto::BADGE_TYPE_NONE
                            : BadgedProfilePhoto::BADGE_TYPE_SYNC_OFF,
          account_icon),
      card_title, base::ASCIIToUTF16(account_->email));
  views::MdTextButton* text_button = nullptr;

  hover_button->SetProperty(views::kBoxLayoutFlexKey,
                            views::BoxLayoutFlexSpecification());
  views::BoxLayout::CrossAxisAlignment alignment =
      views::BoxLayout::CrossAxisAlignment::kCenter;
  if (orientation == views::BoxLayout::Orientation::kVertical) {
    // Set the view to take the whole width of the bubble.
    hover_button->SetPreferredSize(
        gfx::Size(views::LayoutProvider::Get()->GetDistanceMetric(
                      views::DISTANCE_BUBBLE_PREFERRED_WIDTH),
                  GetPreferredSize().height()));
    hover_button->SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                                       views::style::STYLE_SECONDARY);
    // This will place the sign in button at the horizontal end of the bubble.
    alignment = views::BoxLayout::CrossAxisAlignment::kEnd;
  }
  button_layout->set_cross_axis_alignment(alignment);

  views::Builder<BubbleSignInPromoSignInButtonView>(this)
      .SetLayoutManager(std::move(button_layout))
      .AddChildren(views::Builder<HoverButton>(std::move(hover_button))
                       .SetBorder(std::unique_ptr<views::Border>(nullptr))
                       .SetEnabled(false),
                   views::Builder<views::MdTextButton>()
                       .SetText(button_text)
                       .SetStyle(ui::ButtonStyle::kProminent)
                       .CopyAddressTo(&text_button))
      .BuildChildren();

  if (!button_accessibility_text.empty()) {
    text_button->GetViewAccessibility().SetName(
        std::move(button_accessibility_text));
  }

  // Add the callback to the button with a delay if it is an autofill sign in
  // promo.
  AddOrDelayCallbackForSignInButton(access_point, text_button, callback);

  SetProperty(views::kElementIdentifierKey, kPromoSignInButton);
}

void BubbleSignInPromoSignInButtonView::AddOrDelayCallbackForSignInButton(
    signin_metrics::AccessPoint access_point,
    views::MdTextButton* text_button,
    views::Button::PressedCallback& callback) {
  // If the promo is triggered from an autofill bubble, ignore any interaction
  // with the sign in button at first, because the button for an autofill data
  // save is in the same place as the sign in button. If a user double clicked
  // on the save button, it would therefore sign them in directly, or redirect
  // them to a sign in page. The delayed adding of the callback to the button
  // avoids that.
  if (access_point ==
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE) {
    // Add the callback to the button after the delay.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &BubbleSignInPromoSignInButtonView::AddCallbackToSignInButton,
            weak_ptr_factory_.GetWeakPtr(), text_button, std::move(callback)),
        kDoubleClickSignInPreventionDelay);
  } else {
    // Add the callback to the button immediately.
    AddCallbackToSignInButton(text_button, std::move(callback));
  }
}

void BubbleSignInPromoSignInButtonView::AddCallbackToSignInButton(
    views::MdTextButton* text_button,
    views::Button::PressedCallback callback) {
  text_button->SetCallback(std::move(callback));

  // Triggers an event for testing.
  if (GetWidget()) {
    views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
        kBubbleSignInPromoSignInButtonHasCallback, this);
  }
}

BubbleSignInPromoSignInButtonView::~BubbleSignInPromoSignInButtonView() =
    default;

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleSignInPromoSignInButtonHasCallback);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BubbleSignInPromoSignInButtonView,
                                      kPromoSignInButton);

BEGIN_METADATA(BubbleSignInPromoSignInButtonView)
END_METADATA

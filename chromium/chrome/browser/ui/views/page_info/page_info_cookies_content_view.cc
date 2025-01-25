// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/content_settings/browser/ui/cookie_controls_util.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

using content_settings::CookieControlsUtil;

namespace {
class ThirdPartyCookieLabelWrapper : public views::BoxLayoutView {
  METADATA_HEADER(ThirdPartyCookieLabelWrapper, views::BoxLayoutView)

 public:
  explicit ThirdPartyCookieLabelWrapper(std::unique_ptr<views::View> title) {
    auto* provider = ChromeLayoutProvider::Get();

    const int vertical_margin =
        provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
    const int side_margin =
        provider->GetInsetsMetric(views::INSETS_DIALOG).left();

    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetProperty(views::kMarginsKey,
                gfx::Insets::VH(vertical_margin, side_margin));

    title_ = AddChildView(std::move(title));
  }

  ~ThirdPartyCookieLabelWrapper() override = default;

 private:
  // View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Set the preferred width of the label wrapper to the title width. It
    // ensures that the title isn't truncated and it prevents the container
    // expanding to try to fit the description (which should be wrapped).
    const int title_width =
        title_->GetPreferredSize(views::SizeBounds(title_->width(), {}))
            .width();
    DCHECK(available_size.width() >= title_width);
    const int available_width = available_size.width().is_bounded() &&
                                        available_size.width() > title_width
                                    ? available_size.width().value()
                                    : title_width;
    return views::BoxLayoutView::CalculatePreferredSize(
        views::SizeBounds(available_width, {}));
  }

  raw_ptr<views::View> title_ = nullptr;
};

BEGIN_METADATA(ThirdPartyCookieLabelWrapper)
END_METADATA

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoCookiesContentView,
                                      kCookieDialogButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoCookiesContentView, kCookiesPage);

PageInfoCookiesContentView::PageInfoCookiesContentView(PageInfo* presenter)
    : presenter_(presenter) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // The top and bottom margins should be the same as for buttons shown below.
  const auto button_insets = layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);

  cookies_description_label_ =
      AddChildView(std::make_unique<views::StyledLabel>());

  // In the new UI iteration, description labels are aligned with the icons on
  // the left, not with the bubble title.
  cookies_description_label_->SetProperty(views::kMarginsKey, button_insets);
  cookies_description_label_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_COOKIES_DESCRIPTION_LABEL);
  cookies_description_label_->SetDefaultTextStyle(views::style::STYLE_BODY_5);
  cookies_description_label_->SetDefaultEnabledColorId(
      ui::kColorLabelForegroundSecondary);
  cookies_description_label_->SizeToFit(PageInfoViewFactory::kMinBubbleWidth -
                                        button_insets.width());

  AddThirdPartyCookiesContainer();

  // We need the container to have a placeholder to put the buttons in,
  // to ensure the views order.
  cookies_buttons_container_view_ =
      AddChildView(std::make_unique<PageInfoMainView::ContainerView>());
  cookies_buttons_container_view_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_COOKIES_BUTTONS_CONTAINER);

  presenter_->InitializeUiState(this, base::DoNothing());

  SetProperty(views::kElementIdentifierKey, kCookiesPage);
}

PageInfoCookiesContentView::~PageInfoCookiesContentView() = default;

void PageInfoCookiesContentView::SetInitializedCallbackForTesting(
    base::OnceClosure initialized_callback) {
  if (cookies_dialog_button_) {
    std::move(initialized_callback).Run();
  } else {
    initialized_callback_ = std::move(initialized_callback);
  }
}

void PageInfoCookiesContentView::InitCookiesDialogButton() {
  if (cookies_dialog_button_) {
    return;
  }
  // Get the icon.
  PageInfo::PermissionInfo info;
  info.type = ContentSettingsType::COOKIES;
  info.setting = CONTENT_SETTING_ALLOW;

  cookies_buttons_container_view_->AddChildView(
      PageInfoViewFactory::CreateSeparator(
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW)));

  // Create the cookie button, with a temporary value for the subtitle text
  // since the site count is not yet known.
  cookies_dialog_button_ = cookies_buttons_container_view_->AddChildView(
      std::make_unique<RichHoverButton>(
          base::BindRepeating(
              [](PageInfoCookiesContentView* view) {
                view->presenter_->OpenCookiesDialog();
              },
              this),
          PageInfoViewFactory::GetPermissionIcon(info),
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_DIALOG_BUTTON_TITLE),
          /*secondary_text=*/std::u16string(),
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_COOKIES_DIALOG_BUTTON_TOOLTIP),
          /*subtitle_text=*/u" ", PageInfoViewFactory::GetLaunchIcon()));
  cookies_dialog_button_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG);
  cookies_dialog_button_->SetProperty(views::kElementIdentifierKey,
                                      kCookieDialogButton);
}

void PageInfoCookiesContentView::CookiesSettingsLinkClicked(
    const ui::Event& event) {
  presenter_->OpenCookiesSettingsView();
}

void PageInfoCookiesContentView::SetCookieInfo(
    const CookiesNewInfo& cookie_info) {
  SetDescriptionLabel(cookie_info.blocking_status, cookie_info.enforcement,
                      cookie_info.is_otr);

  SetThirdPartyCookiesInfo(cookie_info);
  InitCookiesDialogButton();
  // Update the text displaying the number of allowed sites.
  cookies_dialog_button_->SetSubtitleText(l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
      cookie_info.allowed_sites_count));

  bool is_fps_allowed = base::FeatureList::IsEnabled(
                            privacy_sandbox::kPrivacySandboxFirstPartySetsUI) &&
                        cookie_info.fps_info;
  SetFpsCookiesInfo(cookie_info.fps_info, is_fps_allowed);

  PreferredSizeChanged();
  if (!initialized_callback_.is_null()) {
    std::move(initialized_callback_).Run();
  }
}

void PageInfoCookiesContentView::SetThirdPartyCookiesTitleAndDescription(
    const CookiesNewInfo& cookie_info) {
  std::u16string title_text;
  int description;
  if (cookie_info.protections_on) {
    title_text =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_TITLE);
    description =
        IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY;
  } else if (cookie_info.expiration.is_null() ||
             cookie_info.enforcement ==
                 CookieControlsEnforcement::kEnforcedByCookieSetting) {
    // Handle permanent site exception.
    title_text = l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_TRACKING_PROTECTION_PERMANENT_ALLOWED_TITLE);
    description =
        IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_PERMANENT_ALLOWED_DESCRIPTION;
  } else {
    // Handle temporary site exception.
    title_text = l10n_util::GetPluralStringFUTF16(
        cookie_info.blocking_status == CookieBlocking3pcdStatus::kLimited
            ? IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_LIMITED_RESTART_TITLE
            : IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_BLOCKED_RESTART_TITLE,
        CookieControlsUtil::GetDaysToExpiration(cookie_info.expiration));
    description = IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_RESTART_DESCRIPTION;
  }
  third_party_cookies_title_->SetText(title_text);
  third_party_cookies_description_->SetText(
      l10n_util::GetStringUTF16(description));
}

void PageInfoCookiesContentView::SetThirdPartyCookiesToggle(
    const CookiesNewInfo& cookie_info) {
  std::u16string subtitle;
  if (cookie_info.protections_on) {
    subtitle = l10n_util::GetStringUTF16(
        cookie_info.blocking_status == CookieBlocking3pcdStatus::kLimited
            ? IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_LIMITED
            : IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_BLOCKED);
  } else {
    subtitle = l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_ALLOWED);
  }
  third_party_cookies_toggle_->SetIsOn(!cookie_info.protections_on);
  third_party_cookies_toggle_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_THIRD_PARTY_COOKIES_TOGGLE);
  third_party_cookies_toggle_->GetViewAccessibility().SetName(subtitle);
  third_party_cookies_toggle_subtitle_->SetText(subtitle);
  third_party_cookies_toggle_subtitle_->SetTextStyle(
      views::style::STYLE_BODY_5);
  third_party_cookies_toggle_subtitle_->SetEnabledColorId(
      ui::kColorLabelForegroundSecondary);
}

void PageInfoCookiesContentView::SetDescriptionLabel(
    CookieBlocking3pcdStatus blocking_status,
    CookieControlsEnforcement enforcement,
    bool is_otr) {
  // Text on cookies description label has an embedded link to cookies settings.
  std::u16string settings_text_for_link = l10n_util::GetStringUTF16(
      blocking_status != CookieBlocking3pcdStatus::kNotIn3pcd
          ? IDS_PAGE_INFO_TRACKING_PROTECTION_SETTINGS_LINK
          : IDS_PAGE_INFO_COOKIES_SETTINGS_LINK);

  size_t offset;
  int description;
  if (blocking_status == CookieBlocking3pcdStatus::kNotIn3pcd) {
    description = IDS_PAGE_INFO_COOKIES_DESCRIPTION;
  } else if (enforcement == CookieControlsEnforcement::kEnforcedByTpcdGrant) {
    description = IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_GRANT_DESCRIPTION;
  } else if (blocking_status == CookieBlocking3pcdStatus::kLimited) {
    description = IDS_PAGE_INFO_TRACKING_PROTECTION_DESCRIPTION;
  } else {
    // Since prefs are set to default in Guest, we won't ever end up in this
    // branch, so `is_otr` means incognito here.
    description =
        is_otr
            ? IDS_PAGE_INFO_TRACKING_PROTECTION_INCOGNITO_BLOCKED_COOKIES_DESCRIPTION
            : IDS_PAGE_INFO_TRACKING_PROTECTION_BLOCKED_COOKIES_DESCRIPTION;
  }
  cookies_description_label_->SetText(
      l10n_util::GetStringFUTF16(description, settings_text_for_link, &offset));

  gfx::Range link_range(offset, offset + settings_text_for_link.length());
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PageInfoCookiesContentView::CookiesSettingsLinkClicked,
          base::Unretained(this)));
  link_style.text_style = views::style::STYLE_LINK_5;
  cookies_description_label_->AddStyleRange(link_range, link_style);
}

void PageInfoCookiesContentView::SetThirdPartyCookiesInfo(
    const CookiesNewInfo& cookie_info) {
  third_party_cookies_container_->SetVisible(cookie_info.controls_visible);
  if (!cookie_info.controls_visible) {
    return;
  }

  SetThirdPartyCookiesTitleAndDescription(cookie_info);
  SetThirdPartyCookiesToggle(cookie_info);
  third_party_cookies_row_->SetIcon(
      PageInfoViewFactory::GetThirdPartyCookiesIcon(
          !cookie_info.protections_on));
  third_party_cookies_row_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_THIRD_PARTY_COOKIES_ROW);

  if (cookie_info.enforcement == CookieControlsEnforcement::kNoEnforcement) {
    third_party_cookies_label_wrapper_->SetVisible(true);
    third_party_cookies_toggle_->SetVisible(true);
    third_party_cookies_enforced_icon_->SetVisible(false);
  } else {
    // In 3PCD, tell the user if they allowed the current site via settings.
    third_party_cookies_label_wrapper_->SetVisible(
        cookie_info.blocking_status != CookieBlocking3pcdStatus::kNotIn3pcd &&
        cookie_info.enforcement ==
            CookieControlsEnforcement::kEnforcedByCookieSetting);
    // In the enforced state, the toggle button is hidden; enforced icon is
    // shown instead of the toggle button.
    third_party_cookies_toggle_->SetVisible(false);
    third_party_cookies_enforced_icon_->SetVisible(true);
    third_party_cookies_enforced_icon_->SetImage(
        PageInfoViewFactory::GetImageModel(
            CookieControlsUtil::GetEnforcedIcon(cookie_info.enforcement)));
    third_party_cookies_enforced_icon_->SetTooltipText(
        CookieControlsUtil::GetEnforcedTooltip(cookie_info.enforcement));
  }
}

void PageInfoCookiesContentView::UpdateBlockingThirdPartyCookiesToggle(
    bool are_cookies_blocked) {
  DCHECK(blocking_third_party_cookies_toggle_);
  blocking_third_party_cookies_toggle_->SetIsOn(are_cookies_blocked);
}

void PageInfoCookiesContentView::OnToggleButtonPressed() {
  presenter_->OnThirdPartyToggleClicked(
      /*block_third_party_cookies=*/!third_party_cookies_toggle_->GetIsOn());
  third_party_cookies_container_->NotifyAccessibilityEvent(
      ax::mojom::Event::kAlert, true);
}

void PageInfoCookiesContentView::SetFpsCookiesInfo(
    std::optional<CookiesFpsInfo> fps_info,
    bool is_fps_allowed) {
  if (is_fps_allowed) {
    InitFpsButton(fps_info->is_managed);
    fps_button_->SetVisible(true);

    const std::u16string fps_button_title =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_FPS_BUTTON_TITLE);
    const std::u16string fps_button_subtitle = l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_FPS_BUTTON_SUBTITLE, fps_info->owner_name);

    // Update the text displaying the name of FPS owner.
    fps_button_->SetTitleText(fps_button_title);
    fps_button_->SetSubtitleText(fps_button_subtitle);
  } else if (fps_button_) {
    fps_button_->SetVisible(false);
  }
  if (!fps_histogram_recorded_) {
    fps_histogram_recorded_ = true;
    base::UmaHistogramBoolean("Security.PageInfo.Cookies.HasFPSInfo",
                              is_fps_allowed);
  }
}

void PageInfoCookiesContentView::InitFpsButton(bool is_managed) {
  if (fps_button_) {
    return;
  }

  // Create the fps_button with temporary values for title and subtitle
  // as we don't have data yet, it will be updated.
  fps_button_ = cookies_buttons_container_view_->AddChildView(
      std::make_unique<RichHoverButton>(
          base::BindRepeating(
              &PageInfoCookiesContentView::FpsSettingsButtonClicked,
              base::Unretained(this)),
          PageInfoViewFactory::GetFpsIcon(),
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES), std::u16string(),
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_FPS_BUTTON_TOOLTIP),
          /*secondary_text=*/u" ", PageInfoViewFactory::GetLaunchIcon(),
          is_managed ? std::optional<ui::ImageModel>(
                           PageInfoViewFactory::GetEnforcedByPolicyIcon())
                     : std::nullopt));
  fps_button_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_FPS_SETTINGS);
}

void PageInfoCookiesContentView::FpsSettingsButtonClicked(ui::Event const&) {
  presenter_->OpenAllSitesViewFilteredToFps();
}

void PageInfoCookiesContentView::AddThirdPartyCookiesContainer() {
  third_party_cookies_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  third_party_cookies_container_->GetViewAccessibility().SetRole(
      ax::mojom::Role::kAlert);
  third_party_cookies_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  third_party_cookies_container_->SetVisible(false);

  third_party_cookies_label_wrapper_ =
      third_party_cookies_container_->AddChildView(
          std::make_unique<ThirdPartyCookieLabelWrapper>(
              views::Builder<views::Label>()
                  .CopyAddressTo(&third_party_cookies_title_)
                  .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                  .SetTextStyle(views::style::STYLE_BODY_3_MEDIUM)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .Build()));

  third_party_cookies_description_ =
      third_party_cookies_label_wrapper_->AddChildView(
          std::make_unique<views::Label>());
  third_party_cookies_description_->SetTextContext(views::style::CONTEXT_LABEL);
  third_party_cookies_description_->SetTextStyle(views::style::STYLE_BODY_5);
  third_party_cookies_description_->SetEnabledColorId(
      ui::kColorLabelForegroundSecondary);
  third_party_cookies_description_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  third_party_cookies_description_->SetMultiLine(true);

  third_party_cookies_row_ = third_party_cookies_container_->AddChildView(
      std::make_unique<RichControlsContainerView>());
  third_party_cookies_row_->SetTitle(l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_COOKIES_THIRD_PARTY_COOKIES_LABEL));
  third_party_cookies_row_->SetIcon(
      PageInfoViewFactory::GetBlockingThirdPartyCookiesIcon());

  third_party_cookies_toggle_subtitle_ =
      third_party_cookies_row_->AddSecondaryLabel(std::u16string());

  third_party_cookies_toggle_ = third_party_cookies_row_->AddControl(
      std::make_unique<views::ToggleButton>(base::BindRepeating(
          &PageInfoCookiesContentView::OnToggleButtonPressed,
          base::Unretained(this))));
  third_party_cookies_enforced_icon_ = third_party_cookies_row_->AddControl(
      std::make_unique<views::ImageView>());
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_chip_button.h"

#include "ash/birch/birch_item.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/wm/overview/birch/birch_bar_constants.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_chip_context_menu_model.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The color and layout parameters of the chip.
constexpr gfx::Insets kInteriorMarginsNoAddon =
    gfx::Insets::TLBR(12, 0, 12, 20);
constexpr gfx::Insets kInteriorMarginsWithAddon = gfx::Insets::VH(12, 0);

// The layout parameters of icon.
constexpr gfx::Insets kIconMargins = gfx::Insets::VH(0, 12);
constexpr int kIconViewSize = 40;
constexpr int kFaviconSize = 16;
constexpr int kFaviconCornerRadius = 20;
constexpr int kCalendarIconSize = 20;
constexpr int kCalendarCornerRadius = 20;
constexpr int kIllustrationSize = 40;
constexpr int kIllustrationCornerRadius = 8;
constexpr int kWeatherImageSize = 32;
constexpr int kErrorImageSize = 32;

// The colors of icons.
constexpr ui::ColorId kIconBackgroundColorId =
    cros_tokens::kCrosSysSystemOnBase;

// The colors and fonts of title and subtitle.
constexpr int kTitleSpacing = 2;
constexpr TypographyToken kTitleFont = TypographyToken::kCrosButton1;
constexpr ui::ColorId kTitleColorId = cros_tokens::kCrosSysOnSurface;
constexpr TypographyToken kSubtitleFont = TypographyToken::kCrosAnnotation1;
constexpr ui::ColorId kSubtitleColorId = cros_tokens::kCrosSysOnSurfaceVariant;

void StylizeIconForItemType(views::ImageView* icon, BirchItemType type) {
  int icon_size;
  int rounded_corners;
  std::optional<ui::ColorId> background_color_id;

  switch (type) {
    case BirchItemType::kCalendar:
      icon_size = kCalendarIconSize;
      rounded_corners = kCalendarCornerRadius;
      background_color_id = kIconBackgroundColorId;
      break;
    case BirchItemType::kWeather:
      icon_size = kWeatherImageSize;
      break;
    case BirchItemType::kReleaseNotes:
      icon_size = kIllustrationSize;
      rounded_corners = kIllustrationCornerRadius;
      background_color_id = kIconBackgroundColorId;
      break;
    default:
      icon_size = kFaviconSize;
      rounded_corners = kFaviconCornerRadius;
      background_color_id = kIconBackgroundColorId;
      break;
  }

  icon->SetImageSize(gfx::Size(icon_size, icon_size));
  icon->SetBorder(
      views::CreateEmptyBorder(gfx::Insets((kIconViewSize - icon_size) / 2)));

  if (background_color_id) {
    icon->SetBackground(views::CreateThemedRoundedRectBackground(
        background_color_id.value(), rounded_corners));
  }
}

BirchSuggestionType GetSuggestionTypeFromItemType(BirchItemType item_type) {
  switch (item_type) {
    case BirchItemType::kWeather:
      return BirchSuggestionType::kWeather;
    case BirchItemType::kCalendar:
      return BirchSuggestionType::kCalendar;
    // Attachments are considered Drive suggestions in the UI.
    case BirchItemType::kAttachment:
    case BirchItemType::kFile:
      return BirchSuggestionType::kDrive;
    // All tab types are "Chrome browser" in the UI.
    case BirchItemType::kTab:
    case BirchItemType::kLastActive:
    case BirchItemType::kMostVisited:
    case BirchItemType::kSelfShare:
      return BirchSuggestionType::kChromeTab;
    case BirchItemType::kLostMedia:
      return BirchSuggestionType::kMedia;
    case BirchItemType::kReleaseNotes:
      return BirchSuggestionType::kExplore;
    default:
      return BirchSuggestionType::kUndefined;
  }
}

}  // namespace

//------------------------------------------------------------------------------
// BirchChipButton::ChipMenuController:
class BirchChipButton::ChipMenuController
    : public views::ContextMenuController {
 public:
  explicit ChipMenuController(BirchChipButton* chip) : chip_(chip) {}
  ChipMenuController(const ChipMenuController&) = delete;
  ChipMenuController& operator=(const ChipMenuController&) = delete;
  ~ChipMenuController() override = default;

 private:
  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
    if (auto* birch_bar_controller_ = BirchBarController::Get()) {
      birch_bar_controller_->ShowChipContextMenu(
          chip_, GetSuggestionTypeFromItemType(chip_->GetItem()->GetType()),
          point, source_type);
    }
  }

  const raw_ptr<BirchChipButton> chip_;
};

//------------------------------------------------------------------------------
// BirchChipButton:
BirchChipButton::BirchChipButton()
    : chip_menu_controller_(std::make_unique<ChipMenuController>(this)) {
  auto flex_layout = std::make_unique<views::FlexLayout>();
  flex_layout_ = flex_layout.get();
  flex_layout_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kInteriorMarginsNoAddon);

  raw_ptr<views::BoxLayoutView> titles_container = nullptr;

  // Build up the chip's contents.
  views::Builder<BirchChipButtonBase>(this)
      .SetLayoutManager(std::move(flex_layout))
      .AddChildren(
          // Icon.
          views::Builder<views::ImageView>().CopyAddressTo(&icon_).SetProperty(
              views::kMarginsKey, kIconMargins),
          // Title and subtitle.
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&titles_container)
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded))
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetBetweenChildSpacing(kTitleSpacing)
              .AddChildren(views::Builder<views::Label>()
                               .CopyAddressTo(&title_)
                               .SetAutoColorReadabilityEnabled(false)
                               .SetEnabledColorId(kTitleColorId)
                               .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                           views::Builder<views::Label>()
                               .CopyAddressTo(&subtitle_)
                               .SetAutoColorReadabilityEnabled(false)
                               .SetEnabledColorId(kSubtitleColorId)
                               .SetHorizontalAlignment(gfx::ALIGN_LEFT)))
      .BuildChildren();

  // Stylize the titles.
  auto* typography_provider = TypographyProvider::Get();
  typography_provider->StyleLabel(kTitleFont, *title_);
  typography_provider->StyleLabel(kSubtitleFont, *subtitle_);

  // Add removal chip panel.
  set_context_menu_controller(chip_menu_controller_.get());
}

BirchChipButton::~BirchChipButton() = default;

void BirchChipButton::Init(BirchItem* item) {
  item_ = item;

  title_->SetText(item_->title());
  subtitle_->SetText(item_->subtitle());

  SetCallback(
      base::BindRepeating(&BirchItem::PerformAction, base::Unretained(item_)));
  if (item_->secondary_action().has_value()) {
    auto* button = SetAddon(std::make_unique<PillButton>(
        base::BindRepeating(&BirchItem::PerformSecondaryAction,
                            base::Unretained(item_)),
        *item_->secondary_action(), PillButton::Type::kSecondaryWithoutIcon));
    button->SetProperty(views::kMarginsKey, gfx::Insets::VH(0, 16));
    button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CALENDAR_JOIN_BUTTON_TOOLTIP));
  }

  StylizeIconForItemType(icon_, item_->GetType());
  item_->LoadIcon(base::BindOnce(&BirchChipButton::SetIconImage,
                                 weak_factory_.GetWeakPtr()));

  SetAccessibleName(item_->title() + u" " + item_->subtitle());
}

const BirchItem* BirchChipButton::GetItem() const {
  return item_.get();
}

BirchItem* BirchChipButton::GetItem() {
  return item_.get();
}

void BirchChipButton::Shutdown() {
  item_ = nullptr;
}

void BirchChipButton::SetIconImage(const ui::ImageModel& icon_image,
                                   bool success) {
  icon_->SetImage(icon_image);

  // Enlarge error icons.
  if (!success) {
    icon_->SetImageSize(gfx::Size(kErrorImageSize, kErrorImageSize));
    icon_->SetBorder(views::CreateEmptyBorder(
        gfx::Insets((kIconViewSize - kErrorImageSize) / 2)));
  }
}

void BirchChipButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureLongPress) {
    // Show removal chip panel.
    gfx::Point screen_location(event->location());
    views::View::ConvertPointToScreen(this, &screen_location);
    ShowContextMenu(screen_location, ui::MENU_SOURCE_TOUCH);
    event->SetHandled();
  }
}

void BirchChipButton::ExecuteCommand(int command_id, int event_flags) {
  auto* birch_bar_controller = BirchBarController::Get();
  CHECK(birch_bar_controller);

  if (command_id ==
      base::to_underlying(BirchBarContextMenuModel::CommandId::kReset)) {
    birch_bar_controller->ExecuteCommand(command_id, event_flags);
    return;
  }

  using CommandId = BirchChipContextMenuModel::CommandId;

  switch (command_id) {
    case base::to_underlying(CommandId::kHideSuggestion):
      birch_bar_controller->OnItemHiddenByUser(item_);
      break;
    case base::to_underlying(CommandId::kHideWeatherSuggestions):
      birch_bar_controller->SetShowSuggestionType(BirchSuggestionType::kWeather,
                                                  /*show=*/false);
      break;
    case base::to_underlying(CommandId::kToggleTemperatureUnits):
      birch_bar_controller->ToggleTemperatureUnits();
      break;
    case base::to_underlying(CommandId::kHideCalendarSuggestions):
      birch_bar_controller->SetShowSuggestionType(
          BirchSuggestionType::kCalendar,
          /*show=*/false);
      break;
    case base::to_underlying(CommandId::kHideDriveSuggestions):
      birch_bar_controller->SetShowSuggestionType(BirchSuggestionType::kDrive,
                                                  /*show=*/false);
      break;
    case base::to_underlying(CommandId::kHideChromeTabSuggestions):
      birch_bar_controller->SetShowSuggestionType(
          BirchSuggestionType::kChromeTab,
          /*show=*/false);
      break;
    case base::to_underlying(CommandId::kHideMediaSuggestions):
      birch_bar_controller->SetShowSuggestionType(BirchSuggestionType::kMedia,
                                                  /*show=*/false);
      break;
    case base::to_underlying(CommandId::kFeedback):
      Shell::Get()->shell_delegate()->OpenFeedbackDialog(
          ShellDelegate::FeedbackSource::kBirch,
          /*description_template=*/std::string(), /*category_tag=*/"fromBirch");
      break;
  }
}

void BirchChipButton::SetAddonInternal(
    std::unique_ptr<views::View> addon_view) {
  if (addon_view_) {
    RemoveChildViewT(addon_view_);
  } else {
    flex_layout_->SetInteriorMargin(kInteriorMarginsWithAddon);
  }
  addon_view_ = AddChildView(std::move(addon_view));
}

BEGIN_METADATA(BirchChipButton)
END_METADATA

}  // namespace ash

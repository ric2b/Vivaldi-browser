// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "ui/views/controls/mute_button.h"

#include "content/public/browser/picture_in_picture_window_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"
#include "vivaldi/app/grit/vivaldi_native_strings.h"
#include "vivaldi/ui/vector_icons/vector_icons.h"

namespace vivaldi {

const int kMuteIconSize = 20;
constexpr SkColor kMuteIconColor = SK_ColorWHITE;

MuteButton::MuteButton(PressedCallback callback)
    : views::ImageButton(std::move(callback)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  // Accessibility.
  SetInstallFocusRingOnFocus(true);

  const std::u16string mute_tab_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_MUTE_TAB_CONTROL_TEXT));
  SetAccessibleName(mute_tab_button_label);
  SetTooltipText(mute_tab_button_label);
}

MuteButton::~MuteButton() {}

void MuteButton::ChangeMode(Mode mode, bool force) {
  if (!force && mode == muted_mode_) {
    return;
  }
  const gfx::VectorIcon& icon =
      mode == Mode::kMute ? kVivaldiMuteMutedIcon : kVivaldiMuteIcon;

  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromImageSkia(gfx::CreateVectorIcon(
                    icon, kMuteIconSize, kMuteIconColor)));

  muted_mode_ = mode;
  SchedulePaint();
}

BEGIN_METADATA(MuteButton)
END_METADATA

}  //  namespace vivaldi

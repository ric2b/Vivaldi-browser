// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "ui/views/controls/volume_slider.h"

#include "content/public/browser/picture_in_picture_window_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"
#include "vivaldi/app/grit/vivaldi_native_strings.h"
#include "vivaldi/ui/vector_icons/vector_icons.h"

namespace vivaldi {

VolumeSlider::VolumeSlider(views::SliderListener* listener)
    : views::Slider(listener) {
  const std::u16string volume_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_VOLUME_CONTROL_TEXT));
  SetAccessibleName(volume_label);
  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);
  SetValue(1);
}

VolumeSlider::~VolumeSlider() = default;

ui::Cursor VolumeSlider::GetCursor(const ui::MouseEvent& event) {
  return ui::mojom::CursorType::kHand;
}

BEGIN_METADATA(VolumeSlider)
END_METADATA

}  //  namespace vivaldi

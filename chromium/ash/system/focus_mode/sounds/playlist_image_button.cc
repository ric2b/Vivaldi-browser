// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/playlist_image_button.h"

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/rounded_rect_cutout_path_builder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace {

constexpr auto kCutoutSize = gfx::SizeF(28.f, 28.f);
constexpr int kCutoutInnerCornerRadius = 16;
constexpr int kCutoutOuterCornerRadius = 10;
constexpr int kSinglePlaylistViewWidth = 72;
constexpr int kIconSize = 20;
constexpr int kMediaActionIconSpacing = 6;
constexpr int kSelectedCurvycutoutSpacing = 4;

std::unique_ptr<lottie::Animation> GetEqualizerAnimation() {
  std::optional<std::vector<uint8_t>> lottie_data =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(
          IDR_FOCUS_MODE_EQUALIZER_ANIMATION);
  CHECK(lottie_data.has_value());

  return std::make_unique<lottie::Animation>(
      cc::SkottieWrapper::UnsafeCreateSerializable(lottie_data.value()));
}

}  // namespace

PlaylistImageButton::PlaylistImageButton() {
  gfx::Size preferred_size(kSinglePlaylistViewWidth, kSinglePlaylistViewWidth);
  SetPreferredSize(preferred_size);

  // Disable the button until we populate it with the correct playlist
  // information.
  SetEnabled(false);

  image_view_ = AddChildView(std::make_unique<views::ImageView>());
  image_view_->SetImageSize(preferred_size);

  selected_curvycutout_icon_ =
      AddChildView(std::make_unique<views::ImageView>());
  selected_curvycutout_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kSelectedIcon, cros_tokens::kCrosSysPrimary, kIconSize));

  lottie_animation_view_ =
      AddChildView(std::make_unique<views::AnimatedImageView>());
  lottie_animation_view_->SetImageSize(gfx::Size(kIconSize, kIconSize));
  lottie_animation_view_->SetAnimatedImage(GetEqualizerAnimation());

  SetIsSelected(false);
  SetIsPlaying(false);
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
}

PlaylistImageButton::~PlaylistImageButton() = default;

views::ProposedLayout PlaylistImageButton::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    layouts.host_size = GetPreferredSize();
    return layouts;
  }
  auto bounds = GetContentsBounds();
  layouts.child_layouts.emplace_back(image_view_.get(),
                                     image_view_->GetVisible(), bounds);

  layouts.child_layouts.emplace_back(
      lottie_animation_view_.get(), lottie_animation_view_->GetVisible(),
      gfx::Rect(bounds.right() - kIconSize - kMediaActionIconSpacing,
                bounds.bottom() - kIconSize - kMediaActionIconSpacing,
                kIconSize, kIconSize));

  layouts.child_layouts.emplace_back(
      selected_curvycutout_icon_.get(),
      selected_curvycutout_icon_->GetVisible(),
      gfx::Rect(kSelectedCurvycutoutSpacing, kSelectedCurvycutoutSpacing,
                kIconSize, kIconSize));
  layouts.host_size =
      gfx::Size(size_bounds.width().value(), size_bounds.height().value());
  return layouts;
}

void PlaylistImageButton::SetIsPlaying(bool is_playing) {
  is_playing_ = is_playing;
  is_playing_ ? lottie_animation_view_->Play() : lottie_animation_view_->Stop();
  lottie_animation_view_->SetVisible(is_playing_);
}

bool PlaylistImageButton::GetIsSelected() const {
  return is_selected_;
}

void PlaylistImageButton::SetIsSelected(bool is_selected) {
  if (is_selected_ == is_selected) {
    return;
  }

  is_selected_ = is_selected;
  selected_curvycutout_icon_->SetVisible(is_selected_);

  RoundedRectCutoutPathBuilder builder(
      gfx::SizeF(kSinglePlaylistViewWidth, kSinglePlaylistViewWidth));
  if (is_selected_) {
    // Add a cutout.
    builder
        .AddCutout(RoundedRectCutoutPathBuilder::Corner::kUpperLeft,
                   kCutoutSize)
        .CutoutOuterCornerRadius(kCutoutOuterCornerRadius)
        .CutoutInnerCornerRadius(kCutoutInnerCornerRadius);
  }
  image_view_->SetClipPath(builder.Build());

  OnPropertyChanged(&is_selected_, views::kPropertyEffectsPaint);
}

void PlaylistImageButton::UpdateContents(const gfx::ImageSkia& image) {
  SetEnabled(true);
  image_view_->SetImage(image);
}

void PlaylistImageButton::OnSetTooltipText(const std::u16string& tooltip_text) {
  // Set the tooltip text for `image_view_` to show the tooltip when hovering on
  // it.
  image_view_->SetTooltipText(tooltip_text);
}

BEGIN_METADATA(PlaylistImageButton)
ADD_PROPERTY_METADATA(bool, IsSelected)
END_METADATA

}  // namespace ash

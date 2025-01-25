// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_main_container_view.h"

#include <memory>
#include <utility>

#include "ash/picker/views/picker_contents_view.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_style.h"
#include "ash/style/system_shadow.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"

namespace ash {
namespace {

std::unique_ptr<views::Separator> CreateSeparator() {
  return views::Builder<views::Separator>()
      .SetOrientation(views::Separator::Orientation::kHorizontal)
      .SetColorId(cros_tokens::kCrosSysSeparator)
      .Build();
}

}  // namespace

PickerMainContainerView::PickerMainContainerView() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      kPickerContainerBackgroundColor, kPickerContainerBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPickerContainerBorderRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kPickerContainerShadowType);
  shadow_->SetRoundedCornerRadius(kPickerContainerBorderRadius);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

PickerMainContainerView::~PickerMainContainerView() = default;

views::View* PickerMainContainerView::GetTopItem() {
  return active_page_->GetTopItem();
}

views::View* PickerMainContainerView::GetBottomItem() {
  return active_page_->GetBottomItem();
}

views::View* PickerMainContainerView::GetItemAbove(views::View* item) {
  if (search_field_view_->Contains(item)) {
    views::View* prev_item = GetNextPickerPseudoFocusableView(
        item, PickerPseudoFocusDirection::kBackward, /*should_loop=*/false);
    return Contains(prev_item) ? prev_item : nullptr;
  }
  // Try to get an item above `item`, skipping items outside of the active page
  // (such as search field buttons).
  return active_page_->GetItemAbove(item);
}

views::View* PickerMainContainerView::GetItemBelow(views::View* item) {
  if (search_field_view_->Contains(item)) {
    views::View* next_item = GetNextPickerPseudoFocusableView(
        item, PickerPseudoFocusDirection::kForward, /*should_loop=*/false);
    return Contains(next_item) ? next_item : nullptr;
  }
  // Try to get an item below `item`, skipping items outside of the active page
  // (such as search field buttons).
  return active_page_->GetItemBelow(item);
}

views::View* PickerMainContainerView::GetItemLeftOf(views::View* item) {
  return active_page_->GetItemLeftOf(item);
}

views::View* PickerMainContainerView::GetItemRightOf(views::View* item) {
  return active_page_->GetItemRightOf(item);
}

bool PickerMainContainerView::ContainsItem(views::View* item) {
  return Contains(item);
}

PickerSearchFieldView* PickerMainContainerView::AddSearchFieldView(
    std::unique_ptr<PickerSearchFieldView> search_field_view) {
  search_field_view_ = AddChildView(std::move(search_field_view));
  return search_field_view_;
}

PickerContentsView* PickerMainContainerView::AddContentsView(
    PickerLayoutType layout_type) {
  switch (layout_type) {
    case PickerLayoutType::kMainResultsBelowSearchField:
      AddChildView(CreateSeparator());
      contents_view_ =
          AddChildView(std::make_unique<PickerContentsView>(layout_type));
      break;
    case PickerLayoutType::kMainResultsAboveSearchField:
      contents_view_ =
          AddChildViewAt(std::make_unique<PickerContentsView>(layout_type), 0);
      AddChildViewAt(CreateSeparator(), 1);
      break;
  }

  contents_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));

  return contents_view_;
}

void PickerMainContainerView::SetActivePage(PickerPageView* page_view) {
  contents_view_->SetActivePage(page_view);
  active_page_ = page_view;
}

BEGIN_METADATA(PickerMainContainerView)
END_METADATA

}  // namespace ash

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/overview_desk_bar_view.h"

#include "ash/ash_element_identifiers.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace ash {

OverviewDeskBarView::OverviewDeskBarView(
    base::WeakPtr<OverviewGrid> overview_grid,
    base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator)
    : DeskBarViewBase(overview_grid->root_window(),
                      DeskBarViewBase::Type::kOverview,
                      window_occlusion_calculator) {
  SetProperty(views::kElementIdentifierKey, kOverviewDeskBarElementId);
  overview_grid_ = overview_grid;
}

gfx::Size OverviewDeskBarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // For overview bar, it always come with the fixed width (the full available
  // width).
  return {GetAvailableBounds().width(),
          GetPreferredBarHeight(root_, type_, state_)};
}

gfx::Rect OverviewDeskBarView::GetAvailableBounds() const {
  // Information is retrieved from the widget as it comes with the full
  // available bounds at initialization time and remains unchanged.
  return GetWidget()->GetRootView()->bounds();
}

BEGIN_METADATA(OverviewDeskBarView)
END_METADATA

}  // namespace ash

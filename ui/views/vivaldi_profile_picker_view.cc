// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "ui/views/profile_picker_frame.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

#if !BUILDFLAG(IS_MAC)
#include "ui/aura/window.h"
#endif

std::unique_ptr<views::FrameView> ProfilePickerView::CreateFrameView(
    views::Widget* widget) {
#if BUILDFLAG(IS_MAC)
  return views::WidgetDelegate::CreateFrameView(widget);
#else
  if (!vivaldi::IsVivaldiRunning() || widget->ShouldUseNativeFrame()) {
    return views::WidgetDelegate::CreateFrameView(widget);
  }
  uses_vivaldi_frame_ = true;
  return std::make_unique<vivaldi::PickerFrame>(this);
#endif
}

void ProfilePickerView::DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents)
{
#if !BUILDFLAG(IS_MAC)
  if (!vivaldi::IsVivaldiRunning()) {
    return;
  }
  draggable_region_ = std::make_unique<SkRegion>();
  for (auto& region: regions) {
    draggable_region_->op(
        SkIRect::MakeLTRB(region->bounds.x(), region->bounds.y(),
                          region->bounds.right(), region->bounds.bottom()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }
#endif
}

const SkRegion* ProfilePickerView::GetDraggableRegion() const {
  return draggable_region_.get();
}

void ProfilePickerView::InitVivaldiFrame() {
#if !BUILDFLAG(IS_MAC)
  if (!vivaldi::IsVivaldiRunning())
    return;
  if (!uses_vivaldi_frame_)
    return;
  constexpr int kMouseEdge = 5;   // hover/precise
  constexpr int kTouchEdge = 12;  // fatter for touch, if you care
  aura::Window* top = GetWidget()->GetNativeWindow();
  top->SetEventTargeter(std::make_unique<wm::EasyResizeWindowTargeter>(
      gfx::Insets::TLBR(kMouseEdge, kMouseEdge, kMouseEdge, kMouseEdge),
      gfx::Insets::TLBR(kTouchEdge, kTouchEdge, kTouchEdge, kTouchEdge)));
#endif
}

bool ProfilePickerView::UsesVivaldiFrame() const {
  return uses_vivaldi_frame_;
}

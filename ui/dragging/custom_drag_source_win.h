// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef UI_DRAGGING_CUSTOM_DRAG_SOURCE_WIN_H_
#define UI_DRAGGING_CUSTOM_DRAG_SOURCE_WIN_H_

#include <objidl.h>
#include <wrl/implements.h>

#include "base/memory/ref_counted.h"
#include "ui/base/dragdrop/drag_source_win.h"

namespace vivaldi {

class OSExchangeData;

// A base IDropSource implementation. Handles notifications sent by an active
// drag-drop operation as the user mouses over other drop targets on their
// system. This object tells Windows whether or not the drag should continue,
// and supplies the appropriate cursors.
class COMPONENT_EXPORT(UI_BASE) CustomDragSourceWin : public ui::DragSourceWin {
 public:
  CustomDragSourceWin() = default;
  explicit CustomDragSourceWin(bool dragging_in_progress);
  ~CustomDragSourceWin() override = default;
  CustomDragSourceWin(const CustomDragSourceWin&) = delete;
  CustomDragSourceWin& operator=(const CustomDragSourceWin&) = delete;

  // IDropSource implementation:
  HRESULT __stdcall GiveFeedback(DWORD effect) override;

 private:
  bool custom_tab_dragging_in_progress_ = false;
};

}  // namespace vivaldi

#endif  // UI_DRAGGING_CUSTOM_DRAG_SOURCE_WIN_H_

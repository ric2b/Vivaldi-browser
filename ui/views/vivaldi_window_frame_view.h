// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIEWS_VIVALDI_WINDOW_FRAME_VIEW_H_
#define UI_VIEWS_VIVALDI_WINDOW_FRAME_VIEW_H_

#include <memory>

class VivaldiBrowserWindow;

namespace views {
class NonClientFrameView;
}

std::unique_ptr<views::NonClientFrameView> CreateVivaldiWindowFrameView(
    VivaldiBrowserWindow* window);

#endif  // UI_VIEWS_VIVALDI_WINDOW_FRAME_VIEW_H_

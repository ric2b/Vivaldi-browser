// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIEWS_VIVALDI_NATIVE_WIDGET_H_
#define UI_VIEWS_VIVALDI_NATIVE_WIDGET_H_

#include <memory>

class VivaldiBrowserWindow;

namespace views {
class NativeWidget;
}

std::unique_ptr<views::NativeWidget> CreateVivaldiNativeWidget(
    VivaldiBrowserWindow* window);

#endif  // UI_VIEWS_VIVALDI_NATIVE_WIDGET_H_

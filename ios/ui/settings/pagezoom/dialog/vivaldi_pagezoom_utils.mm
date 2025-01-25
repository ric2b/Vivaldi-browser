// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"

// Expose the FontSizeTabHelper methods to the Objective-C++ code
int FontSizeTabHelper::GetFontZoomSize() const {
  return GetFontSize();
}

void FontSizeTabHelper::SetPageZoomSize(int size) {
  SetPageFontSize(size);
}
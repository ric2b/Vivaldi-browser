// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#import "ios/reader_mode/reader_mode_controller.h"

#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_floating_ui.h"
#import "ios/web/public/web_state.h"

// Helper to safely cast the retained opaque pointer into the Obj-C UI object.
// The object is retained with __bridge_retained when created, and released
// with CFRelease in RemoveFloatingUI().
namespace {
VivaldiReaderModeFloatingUI* GetUI(void* state) {
  return (__bridge VivaldiReaderModeFloatingUI*)state;
}
}  // namespace

void ReaderModeController::CreateFloatingUI() {
  if (!web_state_) {
    return;
  }
  UIView* container = web_state_->GetView();
  if (!container) {
    return;
  }
  // Lazily allocate the Objective-C UI object and retain it into the opaque
  // void* slot. We manually balance this with CFRelease in RemoveFloatingUI().
  if (!objc_ui_state_) {
    VivaldiReaderModeFloatingUI* ui =
        [[VivaldiReaderModeFloatingUI alloc]
                  initWithContainerProvider:^UIView*{
          return web_state_ ? web_state_->GetView() : (UIView*)nil;
        }];
    objc_ui_state_ = (__bridge_retained void*)ui;
  }
  [GetUI(objc_ui_state_) configureWithBrowserPointer:browser_
                                               visible:is_reader_mode_enabled_];
}

void ReaderModeController::RemoveFloatingUI() {
  if (objc_ui_state_) {
    [GetUI(objc_ui_state_) invalidate];
    CFRelease(objc_ui_state_);
    objc_ui_state_ = nullptr;
  }
}

void ReaderModeController::PresentReaderModeUI(void* anchor_view_ptr) {
  CreateFloatingUI();
  if (!objc_ui_state_) {
    return;
  }
  [GetUI(objc_ui_state_) presentFromAnchor:(__bridge UIView*)anchor_view_ptr];
}
// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_FLOATING_UI_H_
#define IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_FLOATING_UI_H_

#import <UIKit/UIKit.h>

// Small Objective-C bridge that owns the floating button and presentation.
// Designed to be referenced from C++ via a void* pointer
@interface VivaldiReaderModeFloatingUI : NSObject

// Initializes with a provider that returns the container view (e.g., web view).
- (instancetype)initWithContainerProvider:(UIView* (^)(void))containerProvider;

// Ensure container, set browser, and update visibility in one call.
- (void)configureWithBrowserPointer:(void*)browserPtr visible:(BOOL)visible;

// Present the settings UI anchored to the given view or the button if nil.
- (void)presentFromAnchor:(UIView*)anchor;

// Invalidate UI and clear strong references.
- (void)invalidate;

@end

#endif  // IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_FLOATING_UI_H_

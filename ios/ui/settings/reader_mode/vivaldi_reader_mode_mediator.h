// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_MEDIATOR_H_
#define IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_consumer.h"

class Browser;
class PrefService;
class ProfileIOS;

namespace web {
class WebState;
}

// Mediator for the reader mode settings view.
@interface VivaldiReaderModeMediator : NSObject <VivaldiReaderModeConsumer>

// The browser associated with this mediator.
@property(nonatomic, assign) Browser* browser;

// The reader mode consumer that handles UI updates.
@property(nonatomic, weak) id<VivaldiReaderModeConsumer> consumer;

// Initializes with the preference service.
- (instancetype)initWithBrowser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

// Default initializer not available.
- (instancetype)init NS_UNAVAILABLE;

// Disconnect from observing pref changes.
- (void)disconnect;
@end

#endif  // IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_MEDIATOR_H_

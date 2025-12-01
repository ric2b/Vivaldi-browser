// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_CONSUMER_H_
#define IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_CONSUMER_H_

// Protocol for mediator to communicate to the Swift implementation.
@protocol VivaldiReaderModeConsumer <NSObject>

// Sets the reader mode enabled state.
- (void)setReaderModeEnabled:(BOOL)enabled;

// Sets the font size for reader mode.
- (void)setFontSize:(int)size;

// Sets the font family for reader mode.
- (void)setFontFamily:(NSString*)family;

// Sets the theme for reader mode.
- (void)setTheme:(NSString*)theme;

// Requests an update of the UI with current preference values.
- (void)updateUIFromPrefs;

@end

#endif  // IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_CONSUMER_H_

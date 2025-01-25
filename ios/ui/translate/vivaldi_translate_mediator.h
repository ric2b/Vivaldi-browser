// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_MEDIATOR_H_
#define IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_MEDIATOR_H_

#import "ios/ui/translate/vivaldi_translate_view_delegate.h"

class ProfileIOS;
@protocol VivaldiTranslateConsumer;

// The mediator for translate view
@interface VivaldiTranslateMediator: NSObject<VivaldiTranslateViewDelegate>

- (instancetype)initWithSelectedText:(NSString*)selectedText
                             profile:(ProfileIOS*)profile
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer of the mediator.
@property(nonatomic, weak) id<VivaldiTranslateConsumer> consumer;

// Disconnects settings and observation.
- (void)disconnect;

@end

#endif  // IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_MEDIATOR_H_

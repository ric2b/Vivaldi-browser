// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_VIEW_DELEGATE_H_
#define IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

/// A protocol implemented by mediator to observe changes
/// from the view.
@protocol VivaldiTranslateViewDelegate

- (void)didSelectTranslateWith:(NSString*)sourceText
                    sourceLang:(NSString*)sourceLang
                      destLang:(NSString*)destLang
              autoDetectSource:(BOOL)autoDetectSource;

@end

#endif  // IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_VIEW_DELEGATE_H_

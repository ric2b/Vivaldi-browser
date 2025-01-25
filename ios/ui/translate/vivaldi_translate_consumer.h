// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_CONSUMER_H_
#define IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_CONSUMER_H_

#import "ios/ui/translate/vivaldi_translate_language_item.h"

/// A protocol implemented by translate view to observe changes
/// from mediator.
@protocol VivaldiTranslateConsumer

- (void)supportedLanguageListDidLoad:
    (NSArray<VivaldiTranslateLanguageItem*>*)languages;

- (void)translationWillBeginWithSourceText:(NSString*)sourceText
            sourceLang:(VivaldiTranslateLanguageItem*)sourceLange
                destLang:(VivaldiTranslateLanguageItem*)sourceLang;

- (void)translationDidFinishWithSourceText:(NSString*)sourceText
          translatedText:(NSString*)translatedText
              sourceLang:(VivaldiTranslateLanguageItem*)sourceLange
                  destLang:(VivaldiTranslateLanguageItem*)sourceLang
                      autoDetectSource:(BOOL)autoDetectSource;

- (void)translationDidFail;

@end

#endif  // IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_CONSUMER_H_

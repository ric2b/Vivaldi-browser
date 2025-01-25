// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_LANGUAGE_ITEM_H_
#define IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_LANGUAGE_ITEM_H_

#import <Foundation/Foundation.h>

// Contains the model data for a language in the Translate page.
@interface VivaldiTranslateLanguageItem : NSObject

  // The display name of the language in the current locale.
@property(nonatomic, strong) NSString* displayName;

  // The display name of the language in the language locale.
@property(nonatomic, strong) NSString* nativeDisplayName;

// The language code for this language.
@property(nonatomic, strong) NSString* languageCode;

// Whether the language is the Translate target language.
@property(nonatomic, assign, getter=isTargetLanguage) BOOL targetLanguage;

// Whether the language is supported by the Translate server.
@property(nonatomic, assign) BOOL supportsTranslate;

@end

#endif  // IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_LANGUAGE_ITEM_H_

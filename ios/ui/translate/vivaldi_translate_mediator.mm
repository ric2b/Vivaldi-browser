// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/translate/vivaldi_translate_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/language/core/browser/language_model_manager.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "ios/chrome/browser/language/model/language_model_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/translate/vivaldi_ios_translate_client.h"
#import "ios/translate/vivaldi_ios_translate_server_request.h"
#import "ios/translate/vivaldi_ios_translate_service.h"
#import "ios/ui/translate/vivaldi_translate_constants.h"
#import "ios/ui/translate/vivaldi_translate_consumer.h"
#import "ios/ui/translate/vivaldi_translate_language_item.h"

@interface VivaldiTranslateMediator () {

  // Profile for getting the prefs
  ProfileIOS* _profile;

  // Preference service from the browser context
  PrefService* _prefs;

  // Translate wrapper for the PrefService.
  std::unique_ptr<translate::TranslatePrefs> _translatePrefs;

  // Translation request instance
  std::unique_ptr<
      vivaldi::VivaldiIOSTranslateServerRequest> _translationRequest;

  // The LanguageModelManager for the translation
  language::LanguageModelManager* _languageModelManager;
}

// Initial text if any when mediator started. Non nil for cases when
// view is presented from partial translation.
@property(nonatomic, strong) NSString* selectedText;

// Translated string from the provided source string
@property(nonatomic, strong) NSString* translatedText;

// Source language either selected, or nil for the case of Auto detect
@property(nonatomic, strong) VivaldiTranslateLanguageItem* sourceLanguage;

// Destination language, non nil.
@property(nonatomic, strong) VivaldiTranslateLanguageItem* destinationLanguage;

// Cached supported languages
@property (nonatomic, strong)
  NSArray<VivaldiTranslateLanguageItem*> *supportedLanguages;
@property (nonatomic, strong)
  NSDictionary<NSString*, VivaldiTranslateLanguageItem*> *languageCodeToItemMap;

@end

@implementation VivaldiTranslateMediator

- (instancetype)initWithSelectedText:(NSString*)selectedText
                             profile:(ProfileIOS*)profile {
  if (self == [super init]) {
    _selectedText = selectedText;
    _profile = profile;
    _prefs = profile->GetPrefs();

    _translatePrefs =
        VivaldiIOSTranslateClient::CreateTranslatePrefs(_prefs);
    _languageModelManager =
        LanguageModelManagerFactory::GetForProfile(profile);

    // Initialize supported languages cache
    [self initializeSupportedLanguages];

    std::string targetLanguageCode =
        VivaldiIOSTranslateService::GetTargetLanguage(
            _prefs, _languageModelManager->GetPrimaryModel());
    self.destinationLanguage = [self languageForCode:targetLanguageCode];

    // Notify consumers so that translate view is updated with source,
    // destination language and initial text.
    [self notifyConsumersIfNeeded];

    // Start the translation right away if there is a text passed to the
    // mediator while creation. This means translate view is opened
    // from web view context menu with selected text.
    if ([_selectedText length] > 0) {
      [self didSelectTranslateWith:_selectedText
                        sourceLang:@""
                          destLang:base::SysUTF8ToNSString(targetLanguageCode)
                  autoDetectSource:YES];
    }

  }
  return self;
}

- (void)disconnect {
  self.selectedText = nil;
  self.translatedText = nil;
  self.sourceLanguage = nil;
  self.destinationLanguage = nil;
  self.supportedLanguages = nil;
  self.languageCodeToItemMap = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiTranslateConsumer>)consumer {
  _consumer = consumer;

  // Notify consumers
  [self notifyConsumersIfNeeded];
}

#pragma mark - Private Helpers

- (void)notifyConsumersIfNeeded {
  [self.consumer supportedLanguageListDidLoad:self.supportedLanguages];

  // Notify consumer that translation will begin
  [self.consumer translationWillBeginWithSourceText:self.selectedText
                                    sourceLang:nil
                                      destLang:self.destinationLanguage];
}

- (void)initializeSupportedLanguages {
  // Get the supported languages.
  std::vector<translate::TranslateLanguageInfo> languages;
  translate::TranslatePrefs::GetLanguageInfoList(
      GetApplicationContext()->GetApplicationLocale(),
      _translatePrefs->IsTranslateAllowedByPolicy(), &languages);

  std::vector<std::string> language_codes;
  translate::TranslateDownloadManager::GetSupportedLanguages(
      _translatePrefs->IsTranslateAllowedByPolicy(), &language_codes);

  // Convert language_codes vector to an unordered_set for lookups.
  std::unordered_set<std::string> language_codes_set(language_codes.begin(),
                                                     language_codes.end());

  NSMutableArray<VivaldiTranslateLanguageItem*> *supportedLanguages =
      [NSMutableArray array];
  NSMutableDictionary<NSString*, VivaldiTranslateLanguageItem*> *languageMap =
      [NSMutableDictionary dictionary];

  for (const auto& language : languages) {
    if (language_codes_set.find(language.code) !=
        language_codes_set.end() && language.supports_translate) {
      VivaldiTranslateLanguageItem *languageItem =
          [self languageItemFromLanguage:language];
      [supportedLanguages addObject:languageItem];
      languageMap[languageItem.languageCode] = languageItem;
    }
  }

  self.supportedLanguages = [supportedLanguages copy];
  self.languageCodeToItemMap = [languageMap copy];
}

- (void)handleTranslationResultWithError:(vivaldi::TranslateError)error
                          sourceLanguage:(const std::string&)sourceLanguage
                     destinationLanguage:(const std::string&)destinationLanguage
                              sourceText:(NSString*)sourceText
                          translatedText:(NSString*)translatedText
                        autoDetectSource:(BOOL)autoDetectSource {

  if (error != vivaldi::TranslateError::kNoError) {
    self.translatedText = nil;
    [self.consumer translationDidFail];
    return;
  }

  self.translatedText = translatedText;
  self.sourceLanguage = [self languageForCode:sourceLanguage];
  self.destinationLanguage = [self languageForCode:destinationLanguage];

  [self.consumer translationDidFinishWithSourceText:sourceText
                                     translatedText:translatedText
                                         sourceLang:self.sourceLanguage
                                           destLang:self.destinationLanguage
                                   autoDetectSource:autoDetectSource];
}

- (NSArray<VivaldiTranslateLanguageItem*>*)supportedLanguagesItems {
  return self.supportedLanguages;
}

- (VivaldiTranslateLanguageItem*)languageForCode:(const std::string&)code {
  NSString *codeNSString = base::SysUTF8ToNSString(code);
  return self.languageCodeToItemMap[codeNSString];
}

- (VivaldiTranslateLanguageItem*)languageItemFromLanguage:
    (const translate::TranslateLanguageInfo&)language {
  VivaldiTranslateLanguageItem* languageItem =
      [[VivaldiTranslateLanguageItem alloc] init];
  languageItem.languageCode = base::SysUTF8ToNSString(language.code);
  languageItem.displayName = base::SysUTF8ToNSString(language.display_name);
  languageItem.nativeDisplayName =
      base::SysUTF8ToNSString(language.native_display_name);
  languageItem.supportsTranslate = language.supports_translate;
  return languageItem;
}

- (void)splitSourceText:(NSString *)sourceText
          intoDataArray:(std::vector<std::string> &)data {
  NSMutableArray<NSString *> *chunks = [NSMutableArray array];
  NSMutableString *currentChunk = [NSMutableString string];

  [sourceText enumerateSubstringsInRange:NSMakeRange(0, sourceText.length)
                                 options:NSStringEnumerationByWords |
                                      NSStringEnumerationSubstringNotRequired
                              usingBlock:^(
    NSString *substring, NSRange substringRange,
          NSRange enclosingRange, BOOL *stop) {

    // Get the range of the current word
    NSRange wordRange = enclosingRange;
    // Extract the word
    NSString *word = [sourceText substringWithRange:wordRange];

    if (currentChunk.length + word.length > kMaxCharactersLimitPerChunk) {
      // Current chunk is full, add it to the chunks array
      [chunks addObject:[currentChunk copy]];
      // Reset current chunk
      [currentChunk setString:@""];
    }

    // Check if the word itself exceeds the max length
    if (word.length > kMaxCharactersLimitPerChunk) {
      // Handle long words (rare case)
      [self splitLongWord:word intoChunks:chunks
           maxChunkLength:kMaxCharactersLimitPerChunk];
    } else {
      [currentChunk appendString:word];
    }
  }];

  // Add the last chunk if it's not empty
  if (currentChunk.length > 0) {
    [chunks addObject:[currentChunk copy]];
  }

  // Convert the chunks to std::string and store in the data vector
  for (NSString *chunk in chunks) {
    std::string utf8String = base::SysNSStringToUTF8(chunk);
    data.push_back(utf8String);
  }
}

- (void)splitLongWord:(NSString *)word
           intoChunks:(NSMutableArray<NSString *> *)chunks
       maxChunkLength:(NSUInteger)maxChunkLength {
  NSMutableString *currentChunk = [NSMutableString string];
  [word enumerateSubstringsInRange:NSMakeRange(0, word.length)
        options:NSStringEnumerationByComposedCharacterSequences
             usingBlock:^(NSString *substring, NSRange substringRange,
                          NSRange enclosingRange, BOOL *stop) {
    if (currentChunk.length + substring.length > maxChunkLength) {
      [chunks addObject:[currentChunk copy]];
      [currentChunk setString:@""];
    }
    [currentChunk appendString:substring];
  }];

  if (currentChunk.length > 0) {
    [chunks addObject:[currentChunk copy]];
  }
}

#pragma mark - VivaldiTranslateViewDelegate

- (void)didSelectTranslateWith:(NSString*)sourceText
                    sourceLang:(NSString*)sourceLang
                      destLang:(NSString*)destLang
              autoDetectSource:(BOOL)autoDetectSource {

  // Convert NSString* to std::string
  std::string source_lang = base::SysNSStringToUTF8(sourceLang);
  std::string dest_lang = base::SysNSStringToUTF8(destLang);

  // Create a vector of strings
  std::vector<std::string> data;
  // Split the strings if needed
  [self splitSourceText:sourceText intoDataArray:data];

  // Create a weak reference to self
  __weak __typeof(self) weakSelf = self;

  // Create the block
  vivaldi::VivaldiTranslateTextCallback callback =
  ^(vivaldi::TranslateError error,
    const std::string& detected_source_language,
    const std::vector<std::string>& source_text,
    const std::vector<std::string>& translated_text) {

    // Capture variables by value
    auto local_error = error;
    auto local_detected_source_language = detected_source_language;
    auto local_source_text = source_text;
    auto local_translated_text = translated_text;

    // Join the source text
    std::string concatenated_source_text = "";
    for (const auto& text : local_source_text) {
      concatenated_source_text += text;
    }

    // Join the translated text
    std::string concatenated_translated_text = "";
    for (const auto& text : local_translated_text) {
      concatenated_translated_text += text;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
      __typeof(self) strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }

      NSString *sourceTextNSString =
          base::SysUTF8ToNSString(concatenated_source_text);
      NSString *translatedTextNSString =
          base::SysUTF8ToNSString(concatenated_translated_text);

      [strongSelf handleTranslationResultWithError:local_error
                                  sourceLanguage:local_detected_source_language
                               destinationLanguage:dest_lang
                                        sourceText:sourceTextNSString
                                    translatedText:translatedTextNSString
                                  autoDetectSource:autoDetectSource];
    });
  };

  // Create the translation request
  _translationRequest =
      std::make_unique<
        vivaldi::VivaldiIOSTranslateServerRequest>(_prefs, callback);

  // Start the request
  _translationRequest->StartRequest(data, source_lang, dest_lang);
}

@end

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONSUMER_H_

#import <UIKit/UIKit.h>

#import <string>

// Vivaldi
#import "components/search_engines/template_url_service.h"
// End Vivaldi

@protocol OmniboxConsumer<NSObject>

// Notifies the consumer to update the autocomplete icon for the currently
// highlighted autocomplete result with given accessibility identifier.
- (void)updateAutocompleteIcon:(UIImage*)icon
    withAccessibilityIdentifier:(NSString*)accessibilityIdentifier;

// Notifies the consumer to update after the search-by-image support status
// changes. (This is usually when the default search engine changes).
- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported;

// Notifies the consumer to update after the Lens support status
// changes. (This is usually when the default search engine changes).
- (void)updateLensImageSupported:(BOOL)lensImageSupported;

/// Sets the name of the search provider.
- (void)setSearchProviderName:(std::u16string)searchProviderName;

// Notifies the consumer to set the following image as an image
// in an omnibox with empty text
- (void)setEmptyTextLeadingImage:(UIImage*)icon;

// Notifies the consumer to update the text immediately.
- (void)updateText:(NSAttributedString*)text;

// Notifies the consumer to update the additional text. Pass `nil` to
// remove additional text.
- (void)updateAdditionalText:(NSAttributedString*)additionalText;

// Vivaldi
@optional
- (void)updateSearchEngineList:
        (std::vector<raw_ptr<TemplateURL, VectorExperimental>>)templateURLs
    currentDefaultSearchEngine:(const TemplateURL*)defaultSearchEngine;
// Notifies the consumer to update UI accordingly when search engine override
// resets.
@optional
- (void)resetOverriddenSearchEngine;
// Updates the state with the enable search engine nickname preference value.
@optional
- (void)setPreferenceForEnableSearchEngineNickname:(BOOL)enable;
// End Vivaldi

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONSUMER_H_

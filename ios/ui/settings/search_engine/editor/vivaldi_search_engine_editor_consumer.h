// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SEARCH_ENGINE_EDITOR_VIVALDI_SEARCH_ENGINE_EDITOR_CONSUMER_H_
#define IOS_UI_SETTINGS_SEARCH_ENGINE_EDITOR_VIVALDI_SEARCH_ENGINE_EDITOR_CONSUMER_H_

#import <Foundation/Foundation.h>

@class VivaldiSearchEngineEditorItem;
@class VivaldiSearchEngineNicknameModel;

// A protocol implemented by consumers to observe view and mediator changes.
NS_SWIFT_UI_ACTOR
@protocol VivaldiSearchEngineEditorConsumer
- (void)searchEngineEditorItemDidChange:(VivaldiSearchEngineEditorItem*)item;

@optional
- (void)availableNicknamesDidUpdate:
    (NSArray<VivaldiSearchEngineNicknameModel*>*)nicknames;
@optional
- (void)searchEngineBackendWillChange;
@optional
- (void)searchEngineBackendDidChange;
@end

#endif  // IOS_UI_SETTINGS_SEARCH_ENGINE_EDITOR_VIVALDI_SEARCH_ENGINE_EDITOR_CONSUMER_H_

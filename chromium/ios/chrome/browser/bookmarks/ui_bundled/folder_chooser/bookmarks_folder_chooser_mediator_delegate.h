// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_MEDIATOR_DELEGATE_H_

@class BookmarksFolderChooserMediator;

// Delegate protocol for the `BookmarksFolderChooserMediator` class.
@protocol BookmarksFolderChooserMediatorDelegate <NSObject>

// Called when folder chooser mediator wants to dismiss the UI.
- (void)bookmarksFolderChooserMediatorWantsDismissal:
    (BookmarksFolderChooserMediator*)mediator;

// Vivaldi
// Called when show only sd folder toggle value is changed in folder chooser
// mediator.
- (void)bookmarksFolderChooserMediatorWantsShowOnlySDFolders:(bool)show;
// End Vivaldi

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_MEDIATOR_DELEGATE_H_

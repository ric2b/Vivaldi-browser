// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_MARKDOWN_VIVALDI_MARKDOWN_CONSTANTS_H_
#define IOS_UI_NOTES_MARKDOWN_VIVALDI_MARKDOWN_CONSTANTS_H_

#import <Foundation/Foundation.h>

extern NSString* vMarkdownMessageHandlerWithReply;
extern NSString* vGetNoteContent;
extern NSString* vGetEditorHeight;
extern NSString* vGetLinkURL;
extern NSString* vGetImageTitleAndURL;
extern NSString* vURLPlaceholderText;
extern NSString* vSetNoteContent;
extern NSString* vOpenLinkEditor;
extern NSString* vLinkEditorUrlField;
extern NSString* vDictMessageCommandField;

// Hyperlink editor actions, must match definitions in
// vivapp/src/mobile/markdown/MarkdownConstants.js
extern NSString* vActionAddLink;
extern NSString* vActionRemoveLink;
extern NSString* vActionUpdateLink;

extern NSString* vCurrentMarkdownFormat;

#endif  // IOS_UI_NOTES_MARKDOWN_VIVALDI_MARKDOWN_CONSTANTS_H_

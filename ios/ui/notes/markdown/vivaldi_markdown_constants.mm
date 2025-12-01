// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/notes/markdown/vivaldi_markdown_constants.h"

#pragma mark - WKScriptMessage names

NSString* vMarkdownMessageHandlerWithReply = @"markdownMessageHandlerWithReply";
NSString* vGetNoteContent = @"getNoteContent";
NSString* vGetEditorHeight = @"getEditorHeight";
NSString* vGetLinkURL = @"getLinkURL";
NSString* vGetImageTitleAndURL = @"getImageTitleAndURL";
NSString* vURLPlaceholderText = @"https://";
NSString* vSetNoteContent = @"setNoteContent";
NSString* vOpenLinkEditor = @"openLinkEditor";
NSString* vLinkEditorUrlField = @"url";
NSString* vDictMessageCommandField = @"command";

// Hyperlink editor actions, must match definitions in
// vivapp/src/mobile/markdown/MarkdownConstants.js
NSString* vActionAddLink = @"add_link";
NSString* vActionRemoveLink = @"remove_link";
NSString* vActionUpdateLink = @"update_link";

NSString* vCurrentMarkdownFormat = @"currentMarkdownFormat";
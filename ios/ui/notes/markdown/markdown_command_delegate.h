// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_MARKDOWN_MARKDOWN_COMMAND_DELEGATE_H_
#define IOS_UI_NOTES_MARKDOWN_MARKDOWN_COMMAND_DELEGATE_H_

@protocol MarkdownCommandDelegate

- (void)sendCommand:(NSString*)command commandType:(NSString*)commandType;

@end

#endif  // IOS_UI_NOTES_MARKDOWN_MARKDOWN_COMMAND_DELEGATE_H_

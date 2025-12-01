// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_TYPE_H_
#define IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_TYPE_H_

// Enum for the Prompt Type
typedef NS_ENUM(NSUInteger, VivaldiSearchEngineChangePromptType) {
  // Normal MultiChoice one
  VivaldiSearchEngineChangePromptTypeMultiChoice = 0,
  // For Donation
  VivaldiSearchEngineChangePromptTypeDonate,
  // When either Donation is denied or user is not donating yet.
  VivaldiSearchEngineChangePromptTypeDonateMultiChoice,
  VivaldiSearchEngineChangePromptTypeNone
};

#endif  // IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_TYPE_H_

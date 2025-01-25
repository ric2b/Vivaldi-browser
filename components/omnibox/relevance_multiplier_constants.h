// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_OMNIBOX_RELEVANCE_MULTIPLIER_CONSTANTS_H_
#define COMPONENTS_OMNIBOX_RELEVANCE_MULTIPLIER_CONSTANTS_H_

// RelevanceMultiplierConstants contains the multiplier value for the
// autocomplete match that used to push the match relevance higher than other
// autocomplete suggestions. A higher multipler value will push that match
// higher and be the default match (if the match is able to be default) than
// a match with lesser multipler value.
namespace RelevanceMultiplierConstants {
inline constexpr int kBookmarksNickname = 10;
inline constexpr int kBookmarks = 9;
}

#endif // COMPONENTS_OMNIBOX_RELEVANCE_MULTIPLIER_CONSTANTS_H_

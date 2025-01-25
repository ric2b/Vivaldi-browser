// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_PANEL_PANEL_CONSTANTS_H_
#define IOS_PANEL_PANEL_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace {

int search_bar_height = 56;
int panel_search_view_height = 56;
int panel_header_height = 112;
int panel_top_view_height = 56;
int panel_sidebar_width = 400;
int panel_top_padding = 12;
int panel_horizontal_padding = 20;
int panel_sheet_corner_radius = 20;
int panel_icon_size = 56;
double panel_transition_duration = 0.3;
double panel_transition_damping = 0.95;

}

extern NSString* vPanelBookmarks;
extern NSString* vPanelBookmarksActive;
extern NSString* vPanelNotes;
extern NSString* vPanelNotesActive;
extern NSString* vPanelHistory;
extern NSString* vPanelHistoryActive;
extern NSString* vPanelReadingList;
extern NSString* vPanelReadingListActive;
extern NSString* vPanelTranslate;
extern NSString* vPanelTranslateActive;

extern NSString* vPanelMoreAction;
extern NSString* vPanelSortOrderAction;

#endif // IOS_PANEL_PANEL_CONSTANTS_H_

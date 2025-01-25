// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_NSD_VIVALDI_NSD_CONSUMER_H_
#define IOS_UI_NTP_NSD_VIVALDI_NSD_CONSUMER_H_

@class VivaldiNSDDirectMatchCategory;
@class VivaldiNSDDirectMatchItem;
@class VivaldiNSDTopSiteItem;

@protocol VivaldiNSDConsumer

/// Notifies consumers with currently selected folder for SD to be
/// added on.
- (void)updateLocationFolder:(NSString*)title
               folderIsGroup:(BOOL)folderIsGroup;

/// Notifies the consumers when top sites are ready.
- (void)popularSitesDidLoad:(NSArray<VivaldiNSDDirectMatchItem*>*)popularSites;

/// Notifies the consumers when direct match category items
/// for selected category is ready
- (void)directMatchCategoryItemsDidLoad:
    (NSArray<VivaldiNSDDirectMatchItem*>*)categoryItems
        forCategory:(VivaldiNSDDirectMatchCategory*)category;

/// Notifies the consumers when top sites are ready.
- (void)topSitesDidLoad:(NSArray<VivaldiNSDTopSiteItem*>*)topSites;

/// Notifies the consumer to update single top site
- (void)topSiteDidUpdate:(VivaldiNSDTopSiteItem*)topSite;

/// Notifies the consumer to dismiss the editor.
- (void)editorWantsDismissal;

@end

#endif  // IOS_UI_NTP_NSD_VIVALDI_NSD_CONSUMER_H_

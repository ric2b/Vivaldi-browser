// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_CUSTOM_VIEWS_VIVALDI_SEARCH_BAR_VIEW_H_
#define IOS_UI_CUSTOM_VIEWS_VIVALDI_SEARCH_BAR_VIEW_H_

#import <UIKit/UIKit.h>

@protocol VivaldiSearchBarViewDelegate
- (void)searchBarTextDidChange:(NSString*)searchText;
@end

// A search bar view to use across application.
@interface VivaldiSearchBarView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// DELEGATE
@property (nonatomic,weak)
  id<VivaldiSearchBarViewDelegate> delegate;

// SETTER
- (void)setPlaceholder:(NSString*)placeholder;
- (void)setSearchText:(NSString*)searchTerms;

@end

#endif  // IOS_UI_CUSTOM_VIEWS_VIVALDI_SEARCH_BAR_VIEW_H_

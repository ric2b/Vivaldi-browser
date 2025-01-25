// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_VIEW_H_
#define ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class ScrollView;
}  // namespace views

namespace ash {

// A selection view that allows users to pick which tabs and apps they want to
// move to a new desk. Its main child is a scroll view that contains many
// `TabAppSelectionItemView`'s representing tabs and apps.
// TODO(http://b/361326120): Add the experimental features view.
// TODO(http://b/361326120): Replace hardcoded values.
// TODO(http://b/361326120): Localize.
class ASH_EXPORT TabAppSelectionView : public views::BoxLayoutView {
  METADATA_HEADER(TabAppSelectionView, views::BoxLayoutView)

 public:
  TabAppSelectionView();
  TabAppSelectionView(const TabAppSelectionView&) = delete;
  TabAppSelectionView& operator=(const TabAppSelectionView&) = delete;
  ~TabAppSelectionView() override;

 private:
  class TabAppSelectionItemView;
  FRIEND_TEST_ALL_PREFIXES(TabAppSelectionViewTest, CloseSelectorItems);

  // We don't use an enum class to avoid too many explicit casts at callsites.
  enum ViewID : int {
    kTabSubtitleID = 1,
    kAppSubtitleID,
    kCloseButtonID,
  };

  void OnCloseButtonPressed(TabAppSelectionItemView* sender);

  raw_ptr<views::ScrollView> scroll_view_;

  std::vector<raw_ptr<TabAppSelectionItemView>> tab_item_views_;
  std::vector<raw_ptr<TabAppSelectionItemView>> app_item_views_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_TAB_APP_SELECTION_VIEW_H_

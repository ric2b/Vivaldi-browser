// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/picker/views/picker_submenu_controller.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"

namespace views {
class ImageView;
class Label;
class View;
}

namespace ash {

class PickerAssetFetcher;
class PickerSearchResult;
class PickerSearchResultsViewDelegate;
class PickerSectionListView;
class PickerSectionView;
class PickerSkeletonLoaderView;

class ASH_EXPORT PickerSearchResultsView : public PickerPageView {
  METADATA_HEADER(PickerSearchResultsView, PickerPageView)

 public:
  // `delegate`, `asset_fetcher` and `submenu_controller` must remain valid for
  // the lifetime of this class.
  explicit PickerSearchResultsView(PickerSearchResultsViewDelegate* delegate,
                                   int picker_view_width,
                                   PickerAssetFetcher* asset_fetcher,
                                   PickerSubmenuController* submenu_controller);
  PickerSearchResultsView(const PickerSearchResultsView&) = delete;
  PickerSearchResultsView& operator=(const PickerSearchResultsView&) = delete;
  ~PickerSearchResultsView() override;

  // The skeleton loader should not be used for short loading times.
  // Wait for a delay before showing the animation.
  static constexpr base::TimeDelta kLoadingAnimationDelay =
      base::Milliseconds(400);

  // PickerPageView:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  // Clears the search results.
  void ClearSearchResults();

  // Append `section` to the current set of search results.
  // TODO: b/325840864 - Merge with existing sections if needed.
  void AppendSearchResults(PickerSearchResultsSection section);

  // Marks that no more search results will be appended until a
  // `ClearSearchResults()` call.
  // Returns whether the "no more results" screen was shown.
  // `illustration` is shown in the center, with `description` shown below it.
  // If `illustration` is empty, then only the description is shown.
  bool SearchStopped(ui::ImageModel illustration, std::u16string description);

  void ShowLoadingAnimation();

  // Returns the index of `inserted_result` in the search result list.
  int GetIndex(const PickerSearchResult& inserted_result);

  PickerSectionListView* section_list_view_for_testing() {
    return section_list_view_;
  }

  base::span<const raw_ptr<PickerSectionView>> section_views_for_testing()
      const {
    return section_views_;
  }

  views::View* no_results_view_for_testing() { return no_results_view_; }
  views::ImageView& no_results_illustration_for_testing() {
    return *no_results_illustration_;
  }
  views::Label& no_results_label_for_testing() { return *no_results_label_; }

  PickerSkeletonLoaderView& skeleton_loader_view_for_testing() {
    return *skeleton_loader_view_;
  }

 private:
  // Runs `select_search_result_callback_` on `result`. Note that only one
  // result can be selected (and subsequently calling this method will do
  // nothing).
  void SelectSearchResult(const PickerSearchResult& result);

  // Adds a result item view to `section_view` based on what type `result` is.
  void AddResultToSection(const PickerSearchResult& result,
                          PickerSectionView* section_view);

  void OnTrailingLinkClicked(PickerSectionType section_type,
                             const ui::Event& event);

  void StopLoadingAnimation();

  raw_ptr<PickerSearchResultsViewDelegate> delegate_;

  // The section list view, contains the section views.
  raw_ptr<PickerSectionListView> section_list_view_ = nullptr;

  // Used to track the views for each section of results.
  std::vector<raw_ptr<PickerSectionView>> section_views_;

  // Used to calculate the index of the inserted result.
  std::vector<PickerSearchResult> top_results_;

  // A view for when there are no results.
  raw_ptr<views::View> no_results_view_ = nullptr;
  raw_ptr<views::ImageView> no_results_illustration_ = nullptr;
  raw_ptr<views::Label> no_results_label_ = nullptr;

  // The skeleton loader view, shown when the results are pending.
  raw_ptr<PickerSkeletonLoaderView> skeleton_loader_view_ = nullptr;

  PickerPreviewBubbleController preview_controller_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_RESULTS_VIEW_H_

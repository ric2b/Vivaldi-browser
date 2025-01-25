// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_BUTTON_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/birch/birch_item.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "base/gtest_prod_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class FlexLayout;
class ImageView;
class Label;
}  // namespace views

namespace ash {

class BirchItem;
class PillButton;

// A compact view of an app, displaying its icon, name, a brief description, and
// an optional call to action.
class ASH_EXPORT BirchChipButton : public BirchChipButtonBase,
                                   public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(BirchChipButton, BirchChipButtonBase)

 public:
  BirchChipButton();
  BirchChipButton(const BirchChipButton&) = delete;
  BirchChipButton& operator=(const BirchChipButton&) = delete;
  ~BirchChipButton() override;

  // BirchChipButtonBase:
  void Init(BirchItem* item) override;
  const BirchItem* GetItem() const override;
  BirchItem* GetItem() override;
  void Shutdown() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  const views::View* addon_view_for_testing() const { return addon_view_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(BirchBarTest, NoCrashOnSettingIconAfterShutdown);
  FRIEND_TEST_ALL_PREFIXES(BirchBarTest, UpdateLostMediaChip);
  class ChipMenuController;

  void SetAddon(std::unique_ptr<views::View> addon_view);

  // Stylizes the icon based on the type of the item, the type of the item's
  // secondary icon, and whether the icon image needs to be minified.
  // `use_smaller_dimension` will only be true for icons loaded via
  // `DownloadImageFromUrl` in `BirchItem`.
  void StylizeIconForItemType(BirchItemType type,
                              SecondaryIconType secondary_icon_type,
                              bool use_smaller_dimension);

  // Sets the item icon.
  void SetIconImage(const ui::ImageModel& icon_image,
                    SecondaryIconType secondary_icon_image);

  // The chip context menu controller.
  std::unique_ptr<ChipMenuController> chip_menu_controller_;

  // The source of the chip.
  raw_ptr<BirchItem> item_ = nullptr;

  // The components owned by the chip view.
  raw_ptr<views::FlexLayout> flex_layout_ = nullptr;
  raw_ptr<views::View> icon_parent_view_ = nullptr;
  raw_ptr<views::ImageView> primary_icon_view_ = nullptr;
  raw_ptr<views::ImageView> secondary_icon_view_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
  raw_ptr<views::View> addon_view_ = nullptr;

  base::WeakPtrFactory<BirchChipButton> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/*no export*/, BirchChipButton, BirchChipButtonBase)
VIEW_BUILDER_METHOD(Init, BirchItem*)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/*no export*/, ash::BirchChipButton)

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_BUTTON_H_

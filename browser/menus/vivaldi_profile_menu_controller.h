//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef BROWSER_MENUS_VIVALDI_PROFILE_MENU_CONTROLLER_H_
#define BROWSER_MENUS_VIVALDI_PROFILE_MENU_CONTROLLER_H_

#include "ui/menus/simple_menu_model.h"

class ProfileAttributesEntry;
class Profile;

namespace content {
struct ContextMenuParams;
}

namespace vivaldi {

class VivaldiRenderViewContextMenu;

class ProfileMenuController {
 public:
  ProfileMenuController(VivaldiRenderViewContextMenu* rv_context_menu,
                        Profile* active_profile,
                        bool is_image);
  ~ProfileMenuController();

  static bool HasRemoteProfile(Profile* active_profile);
  static void CollectTargetProfiles(Profile* active_profile,
                                    std::vector<ProfileAttributesEntry*>& list);

  void Populate(std::u16string label,
                ui::SimpleMenuModel* menu_model,
                ui::SimpleMenuModel::Delegate* delegate);
  bool HandleCommand(int command_id, int event_flags);
  bool IsCommandIdEnabled(int command_id,
                          const content::ContextMenuParams& params,
                          bool* enabled);

 private:
  struct Ids {
    int first;        // First item command id (in range).
    int last;         // Last item command id (in range).
    int menu;         // Sub menu command id.
    int item_string;  // Item string id.
    int menu_string;  // Menu string id,
  };

  Ids GetIds(bool is_image);
  void SetImage(int command_id,
                ui::SimpleMenuModel* menu_model,
                const gfx::Image& icon);

  const raw_ptr<VivaldiRenderViewContextMenu> rv_context_menu_;
  const raw_ptr<Profile> active_profile_;
  bool is_image_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> models_;
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_PROFILE_MENU_CONTROLLER_H_

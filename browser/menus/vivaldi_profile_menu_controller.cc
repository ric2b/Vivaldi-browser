//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "browser/menus/vivaldi_profile_menu_controller.h"

#include "app/vivaldi_resources.h"
#include "browser/menus/vivaldi_menu_enums.h"
#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"

namespace vivaldi {

ProfileMenuController::ProfileMenuController(
    VivaldiRenderViewContextMenu* rv_context_menu,
    Profile* active_profile,
    bool is_image)
    : rv_context_menu_(rv_context_menu),
      active_profile_(active_profile),
      is_image_(is_image) {}

ProfileMenuController::~ProfileMenuController() = default;

// static
bool ProfileMenuController::HasRemoteProfile(Profile* activeProfile) {
  std::vector<ProfileAttributesEntry*> target_profiles_entries;
  CollectTargetProfiles(activeProfile, target_profiles_entries);
  return !target_profiles_entries.empty();
}

// static
void ProfileMenuController::CollectTargetProfiles(
    Profile* activeProfile,
    std::vector<ProfileAttributesEntry*>& list) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager && activeProfile->IsRegularProfile()) {
    std::vector<ProfileAttributesEntry*> entries =
        profile_manager->GetProfileAttributesStorage()
            .GetAllProfilesAttributesSortedByNameWithCheck();
    for (ProfileAttributesEntry* entry : entries) {
      base::FilePath profile_path = entry->GetPath();
      Profile* profile = profile_manager->GetProfileByPath(profile_path);
      if (profile != activeProfile && !entry->IsOmitted() &&
          !entry->IsSigninRequired()) {
        list.push_back(entry);
      }
    }
  }
}

ProfileMenuController::Ids ProfileMenuController::GetIds(bool is_image) {
  Ids ids;
  if (is_image) {
    ids.first = IDC_VIV_OPEN_IMAGE_IN_PROFILE_FIRST;
    ids.last = IDC_VIV_OPEN_IMAGE_IN_PROFILE_LAST;
    ids.menu = IDC_CONTENT_CONTEXT_OPENLINKINPROFILE;
    ids.menu_string = IDS_CONTENT_CONTEXT_OPENLINKINPROFILES;
    ids.item_string = IDS_VIV_OPEN_IMAGE_PROFILE;
  } else {
    ids.first = IDC_OPEN_LINK_IN_PROFILE_FIRST;
    ids.last = IDC_OPEN_LINK_IN_PROFILE_LAST;
    ids.menu = IDC_CONTENT_CONTEXT_OPENLINKINPROFILE;
    ids.menu_string = IDS_CONTENT_CONTEXT_OPENLINKINPROFILES;
    ids.item_string = IDS_VIV_OPEN_LINK_PROFILE;
  }
  return ids;
}

void ProfileMenuController::Populate(std::u16string label,
                                     ui::SimpleMenuModel* menu_model,
                                     ui::SimpleMenuModel::Delegate* delegate) {
  std::vector<ProfileAttributesEntry*> target_profiles_entries;
  CollectTargetProfiles(active_profile_, target_profiles_entries);

  Ids ids(GetIds(is_image_));

  if (target_profiles_entries.size() == 1) {
    int menu_index =
        static_cast<int>(rv_context_menu_->get_profile_link_paths().size());
    int command_id = ids.first + menu_index;

    ProfileAttributesEntry* entry = target_profiles_entries.front();
    rv_context_menu_->get_profile_link_paths().push_back(entry->GetPath());
    menu_model->AddItem(command_id, l10n_util::GetStringFUTF16(
                                        ids.item_string, entry->GetName()));
    SetImage(command_id, menu_model, entry->GetAvatarIcon());
  } else if (target_profiles_entries.size() > 1) {
    ui::SimpleMenuModel* child_menu_model = new ui::SimpleMenuModel(delegate);
    models_.push_back(base::WrapUnique(child_menu_model));
    menu_model->AddSubMenu(ids.menu, label, child_menu_model);

    for (ProfileAttributesEntry* entry : target_profiles_entries) {
      int menu_index =
          static_cast<int>(rv_context_menu_->get_profile_link_paths().size());
      int command_id = ids.first + menu_index;
      // In extreme cases, we might have more profiles than available
      // command ids. In that case, just stop creating new entries - the
      // menu is probably useless at this point already.
      if (command_id > ids.last) {
        break;
      }
      rv_context_menu_->get_profile_link_paths().push_back(entry->GetPath());
      child_menu_model->AddItem(command_id, entry->GetName());
      SetImage(command_id, child_menu_model, entry->GetAvatarIcon());
    }
  }
}

void ProfileMenuController::SetImage(int command_id,
                                     ui::SimpleMenuModel* menu_model,
                                     const gfx::Image& icon) {
  if (icon.Width() < 16 || icon.Height() < 16)
    return;
  int target_dip_width = icon.Width();
  int target_dip_height = icon.Height();
  gfx::CalculateFaviconTargetSize(&target_dip_width, &target_dip_height);
  gfx::Image sized_icon = profiles::GetSizedAvatarIcon(
      icon, target_dip_width, target_dip_height, profiles::SHAPE_CIRCLE);
  menu_model->SetIcon(menu_model->GetIndexOfCommandId(command_id).value(),
                      ui::ImageModel::FromImage(sized_icon));
}

bool ProfileMenuController::HandleCommand(int command_id, int event_flags) {
  Ids ids(GetIds(is_image_));
  if (command_id >= ids.first && command_id <= ids.last) {
    if (is_image_) {
      // Set up so that chrome code can be used to execute the action even
      // for image urls.
      rv_context_menu_->SetLinkUrl(rv_context_menu_->params().src_url);
      Ids link(GetIds(false));
      command_id = link.first + (command_id - ids.first);
    }
    rv_context_menu_->ExecuteCommand(command_id, event_flags);
    return true;
  }
  return false;
}

bool ProfileMenuController::IsCommandIdEnabled(
    int command_id,
    const content::ContextMenuParams& params,
    bool* enabled) {
  Ids ids(GetIds(is_image_));
  if (command_id == ids.menu ||
      (command_id >= ids.first && command_id <= ids.last)) {
    if (is_image_) {
      // The controller is set up for canvas elements as well.
      *enabled =
          params.media_type == blink::mojom::ContextMenuDataMediaType::kImage;
    } else {
      *enabled = true;
    }
    return true;
  }
  return false;
}

}  // namespace vivaldi

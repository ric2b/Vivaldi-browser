// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

// Vivaldi
#include "app/vivaldi_apptools.h"
#include "ui/gfx/platform_font_mac.h"
#include "ui/views/vivaldi_menu_config.h"

namespace views {

void MenuConfig::InitPlatform() {
  context_menu_font_list = font_list = gfx::FontList(gfx::Font( // Vivaldi keep
      new gfx::PlatformFontMac(gfx::PlatformFontMac::SystemFontType::kMenu))); // Vivaldi keep
  check_selected_combobox_item = true;
  arrow_key_selection_wraps = false;
  use_mnemonics = false;
  show_context_menu_accelerators = false;
  all_menus_use_prefix_selection = true;

  if (vivaldi::IsVivaldiRunning()) {
    vivaldi::OverrideMenuConfig(this);
  }
}

}  // namespace views

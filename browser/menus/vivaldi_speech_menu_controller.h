//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef BROWSER_MENUS_VIVALDI_SPEECH_MENU_CONTROLLER_H_
#define BROWSER_MENUS_VIVALDI_SPEECH_MENU_CONTROLLER_H_

#include "ui/menus/simple_menu_model.h"

namespace vivaldi {

class VivaldiRenderViewContextMenu;

class SpeechMenuController {
 public:
  enum MenuCommands {
    kSpeechMenu = 100,
    kSpeechStartSpeaking,
    kSpeechStopSpeaking,
  };

  SpeechMenuController(VivaldiRenderViewContextMenu* rv_context_menu);
  ~SpeechMenuController();

  void Populate(ui::SimpleMenuModel* menu_model);
  bool HandleCommand(int command_id, int event_flags);
  bool IsCommandIdEnabled(int command_id, bool* enabled);

  void SpeakText(const std::u16string& text);
  void StopSpeaking();
  bool IsSpeaking();

 private:
  VivaldiRenderViewContextMenu* rv_context_menu_;
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_PROFILE_MENU_CONTROLLER_H_

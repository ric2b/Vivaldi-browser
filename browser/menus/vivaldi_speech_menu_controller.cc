//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "browser/menus/vivaldi_speech_menu_controller.h"

#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "ui/menus/cocoa/text_services_context_menu.h"
#include "ui/strings/grit/ui_strings.h"

namespace vivaldi {

SpeechMenuController::SpeechMenuController(
    VivaldiRenderViewContextMenu* rv_context_menu)
    : rv_context_menu_(rv_context_menu) {}

SpeechMenuController::~SpeechMenuController() = default;

void SpeechMenuController::Populate(ui::SimpleMenuModel* menu_model) {
  menu_model->AddItemWithStringId(kSpeechStartSpeaking,
                                  IDS_SPEECH_START_SPEAKING_MAC);
  menu_model->AddItemWithStringId(kSpeechStopSpeaking,
                                  IDS_SPEECH_STOP_SPEAKING_MAC);
}

bool SpeechMenuController::HandleCommand(int command_id, int event_flags) {
  switch (command_id) {
    case kSpeechStartSpeaking:
      SpeakText(rv_context_menu_->GetSelectedText());
      return true;
    case kSpeechStopSpeaking:
      StopSpeaking();
      return true;
    default:
      return false;
  }
}

bool SpeechMenuController::IsCommandIdEnabled(int command_id, bool* enabled) {
  switch (command_id) {
    case kSpeechStartSpeaking:
      *enabled = !IsSpeaking() && !rv_context_menu_->GetSelectedText().empty();
      return true;
    case kSpeechStopSpeaking:
      *enabled = IsSpeaking();
      return true;
    default:
      return false;
  };
}

void SpeechMenuController::SpeakText(const std::u16string& text) {
  ui::TextServicesContextMenu::SpeakText(text);
}

void SpeechMenuController::StopSpeaking() {
  ui::TextServicesContextMenu::StopSpeaking();
}

bool SpeechMenuController::IsSpeaking() {
  return ui::TextServicesContextMenu::IsSpeaking();
}

}  // namespace vivaldi
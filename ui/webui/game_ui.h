// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef UI_WEBUI_GAME_UI_H_
#define UI_WEBUI_GAME_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace vivaldi {

// The WebUI handler for chrome://game.
class GameUI : public content::WebUIController {
 public:
  explicit GameUI(content::WebUI* web_ui);
  ~GameUI() override;
  GameUI(const GameUI&) = delete;
  GameUI& operator=(const GameUI&) = delete;
};

}  // namespace vivaldi

#endif  // UI_WEBUI_GAME_UI_H_

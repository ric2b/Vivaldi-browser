// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "ui/webui/game_ui.h"

#include "app/vivaldi_constants.h"
#include "app/vivaldi_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace vivaldi {

// generated from generate_asset_definitions.py
#include "vivaldi_game_resources.inc"

content::WebUIDataSource* CreateGameUIDataSource(content::WebUI* web_ui) {
  content::WebUIDataSource* html_source =
    content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                           vivaldi::kVivaldiGameHost);

  html_source->SetDefaultResource(IDR_VIVALDI_GAME_INDEX);

  CreateGameUIAssets(html_source);

  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://game 'unsafe-inline' 'self';");

  // Allow workers from chrome://game
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src chrome://game;");

  html_source->DisableTrustedTypesCSP();

  return html_source;
}

GameUI::GameUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  CreateGameUIDataSource(web_ui);
}
GameUI::~GameUI() {}

}  // namespace vivaldi

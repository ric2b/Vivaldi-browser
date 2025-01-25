// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_UI_H_

#include "chrome/browser/ui/webui/data_sharing/data_sharing.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"

class DataSharingPageHandler;
class DataSharingUI;

class DataSharingUIConfig : public DefaultTopChromeWebUIConfig<DataSharingUI> {
 public:
  DataSharingUIConfig();
  ~DataSharingUIConfig() override;

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldAutoResizeHost() override;
};

class DataSharingUI : public UntrustedTopChromeWebUIController,
                      public data_sharing::mojom::PageHandlerFactory {
 public:
  explicit DataSharingUI(content::WebUI* web_ui);
  ~DataSharingUI() override;

  void BindInterface(
      mojo::PendingReceiver<data_sharing::mojom::PageHandlerFactory> receiver);

  DataSharingPageHandler* page_handler() { return page_handler_.get(); }

  static constexpr std::string GetWebUIName() { return "DataSharingBubble"; }

 private:
  // data_sharing::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingRemote<data_sharing::mojom::Page> page,
                         mojo::PendingReceiver<data_sharing::mojom::PageHandler>
                             receiver) override;

  std::unique_ptr<DataSharingPageHandler> page_handler_;

  mojo::Receiver<data_sharing::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_UI_H_

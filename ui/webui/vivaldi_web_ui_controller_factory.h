// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#ifndef UI_WEBUI_VIVALDI_WEB_UI_CONTROLLER_FACTORY_H_
#define UI_WEBUI_VIVALDI_WEB_UI_CONTROLLER_FACTORY_H_

#include "base/memory/singleton.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

namespace vivaldi {

class VivaldiWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override;
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

  static VivaldiWebUIControllerFactory* GetInstance();

  static base::RefCountedMemory* GetFaviconResourceBytes(
      const GURL& page_url,
      ui::ResourceScaleFactor scale_factor);

 protected:
  VivaldiWebUIControllerFactory();
  ~VivaldiWebUIControllerFactory() override;
  VivaldiWebUIControllerFactory(const VivaldiWebUIControllerFactory&) = delete;
  VivaldiWebUIControllerFactory& operator=(
      const VivaldiWebUIControllerFactory&) = delete;

 private:
  friend base::DefaultSingletonTraits<VivaldiWebUIControllerFactory>;
};

}  // namespace vivaldi

#endif  // UI_WEBUI_VIVALDI_WEB_UI_CONTROLLER_FACTORY_H_

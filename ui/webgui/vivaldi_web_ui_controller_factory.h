// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "base/memory/singleton.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace vivaldi
{

class VivaldiWebUIControllerFactory : public content::WebUIControllerFactory
{
public:
	content::WebUI::TypeID GetWebUIType(
					content::BrowserContext* browser_context,
					const GURL& url) const override;
	bool UseWebUIForURL(content::BrowserContext* browser_context,
					const GURL& url) const override;
	bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
					const GURL& url) const override;
	content::WebUIController* CreateWebUIControllerForURL(
					content::WebUI* web_ui,
					const GURL& url) const override;

	static VivaldiWebUIControllerFactory *GetInstance();

protected:
	VivaldiWebUIControllerFactory();
	~VivaldiWebUIControllerFactory() override;

private:
	friend struct DefaultSingletonTraits<VivaldiWebUIControllerFactory>;

DISALLOW_COPY_AND_ASSIGN(VivaldiWebUIControllerFactory);
};

}

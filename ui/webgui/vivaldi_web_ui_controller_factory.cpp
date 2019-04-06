// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "ui/webgui/vivaldi_web_ui_controller_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/webgui/notes_ui.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#endif

using content::WebUI;
using content::WebUIController;

// A function for creating a new WebUI. The caller owns the return value, which
// may be NULL (for example, if the URL refers to an non-existent extension).
typedef std::unique_ptr<content::WebUIController>(*WebUIFactoryFunction)(WebUI* web_ui,
                                                 const GURL& url);
namespace vivaldi {

// Template for defining WebUIFactoryFunction.
template <class T>
std::unique_ptr<content::WebUIController> NewVivaldiWebUI(WebUI* web_ui, const GURL& url) {
  return std::make_unique<T>(web_ui);
}

// Returns a function that can be used to create the right type of WebUI for a
// tab, based on its URL. Returns NULL if the URL doesn't have WebUI associated
// with it.
WebUIFactoryFunction GetVivaldiWebUIFactoryFunction(WebUI* web_ui,
                                                    Profile* profile,
                                                    const GURL& url) {
  if (!url.SchemeIs("vivaldi"))
    return NULL;

  if (url.host() == "notes")
    return &NewVivaldiWebUI<NotesUI>;

  return NULL;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Only create ExtensionWebUI for URLs that are allowed extension bindings,
// hosted by actual tabs.
bool NeedsExtensionWebUI(Profile* profile, const GURL& url) {
  if (!profile)
    return false;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)
          ->enabled_extensions()
          .GetExtensionOrAppByURL(url);
  // Allow bindings for all packaged extensions and component hosted apps.
  return extension &&
         (!extension->is_hosted_app() ||
          extension->location() == extensions::Manifest::COMPONENT);
}
#endif

WebUI::TypeID VivaldiWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebUIFactoryFunction function =
      GetVivaldiWebUIFactoryFunction(NULL, profile, url);
  return function ? reinterpret_cast<WebUI::TypeID>(function) : WebUI::kNoWebUI;
}

bool VivaldiWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

bool VivaldiWebUIControllerFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  // Extensions are rendered via WebUI in tabs, but don't actually need WebUI
  // bindings (see the ExtensionWebUI constructor).
  return
#if BUILDFLAG(ENABLE_EXTENSIONS)
      !NeedsExtensionWebUI(Profile::FromBrowserContext(browser_context), url) &&
#endif
      UseWebUIForURL(browser_context, url);
}

std::unique_ptr<content::WebUIController>
VivaldiWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) const {
  Profile* profile = Profile::FromWebUI(web_ui);
  WebUIFactoryFunction function =
      GetVivaldiWebUIFactoryFunction(web_ui, profile, url);
  if (!function)
    return NULL;

  return (*function)(web_ui, url);
}

VivaldiWebUIControllerFactory* VivaldiWebUIControllerFactory::GetInstance() {
  return base::Singleton<VivaldiWebUIControllerFactory>::get();
}

VivaldiWebUIControllerFactory::VivaldiWebUIControllerFactory() {}

VivaldiWebUIControllerFactory::~VivaldiWebUIControllerFactory() {}

}  // namespace vivaldi

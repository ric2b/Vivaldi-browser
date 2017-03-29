// Copyright 2015 Vivaldi Technologies AS. All rights reserved.

// This class is just a proxy for emitting events from the Chrome ui for
// browserAction and pageAction badges.

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/command.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"


namespace extensions {

namespace ToggleBrowserActionVisibility =
    api::extension_action_utils::ToggleBrowserActionVisibility;

#define WINDOWID_PRESTRING "vivaldi_window_"

// Copied from "chrome/browser/devtools/devtools_ui_bindings.cc"
std::string SkColorToRGBAString(SkColor color) {
  // We avoid StringPrintf because it will use locale specific formatters for
  // the double (e.g. ',' instead of '.' in German).
  return "rgba(" + base::IntToString(SkColorGetR(color)) + "," +
         base::IntToString(SkColorGetG(color)) + "," +
         base::IntToString(SkColorGetB(color)) + "," +
         base::DoubleToString(SkColorGetA(color) / 255.0) + ")";
}

/*static*/
void ExtensionActionUtil::BroadcastEvent(const std::string& eventname,
                                         scoped_ptr<base::ListValue> args,
                                         content::BrowserContext* context) {
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::UNKNOWN, eventname, args.Pass()));
  event->restrict_to_browser_context = context;
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(event.Pass());
  }
}

ExtensionActionUtil* ExtensionActionUtilFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionActionUtil*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ExtensionActionUtilFactory* ExtensionActionUtilFactory::GetInstance() {
  return Singleton<ExtensionActionUtilFactory>::get();
}

ExtensionActionUtilFactory::ExtensionActionUtilFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionActionUtils",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
}

ExtensionActionUtilFactory::~ExtensionActionUtilFactory() {
}

ExtensionActionUtil::ExtensionActionUtil(Profile* profile)
    : profile_(profile),
      extension_registry_observer_(this),
      extension_action_api_observer_(this),
      weak_ptr_factory_(this) {
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));
  extension_action_api_observer_.Add(ExtensionActionAPI::Get(profile));
}

ExtensionActionUtil::~ExtensionActionUtil() {
  extension_registry_observer_.RemoveAll();
}

void ExtensionActionUtil::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {

  api::extension_action_utils::ExtensionInfo info;

  // We only update the browseraction items for the active tab.
  int tab_id = ExtensionAction::kDefaultTabId;
  int windowId = extension_misc::kCurrentWindowId;

  Browser* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : nullptr;
  if (browser) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    tab_id = SessionTabHelper::IdForTab(tab_strip->GetActiveWebContents());
    windowId = browser->session_id().id();
  }

  FillInfoForTabId(info, extension_action, tab_id, profile_);

  // We need to fill in windowid to use for differentiating between the
  // different browser windows. Since the event is originating from the
  // tab-strip which is owned by the browserwindow.

  scoped_ptr<base::ListValue> args =
      api::extension_action_utils::OnUpdated::Create(info, windowId, tab_id);

  BroadcastEvent(api::extension_action_utils::OnUpdated::kEventName,
                 args.Pass(), browser_context);

}

/* static */
bool ExtensionActionUtil::GetWindowIdFromExtData(const std::string& extdata,
                                                        std::string& windowId) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
    scoped_ptr<base::Value> json =
        base::JSONReader::Read(extdata, options);
    base::DictionaryValue* dict = NULL;
    if (json && json->GetAsDictionary(&dict)) {
      dict->GetString("ext_id", &windowId);
      windowId = WINDOWID_PRESTRING + windowId;
      return true;
    }
  return false;
}

// Called when there is a change to the extension action's visibility.
void ExtensionActionUtil::OnExtensionActionVisibilityChanged(
    const std::string& extension_id,
    bool is_now_visible) {

  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  api::extension_action_utils::ExtensionInfo info;

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);

  ExtensionAction* action =
      extension ? action_manager->GetExtensionAction(*extension) : nullptr;

  FillInfoForTabId(info, action, ExtensionAction::kDefaultTabId, profile_);

  int windowId = extension_misc::kCurrentWindowId;

  scoped_ptr<base::ListValue> args =
      api::extension_action_utils::OnUpdated::Create(
          info, windowId, ExtensionAction::kDefaultTabId);
  ExtensionActionUtil::BroadcastEvent(
      api::extension_action_utils::OnUpdated::kEventName, args.Pass(),
      static_cast<content::BrowserContext*>(profile_));
}

// Called when the page actions have been refreshed do to a possible change
// in count or visibility.
void ExtensionActionUtil::OnPageActionsUpdated(
    content::WebContents* web_contents) {
}

// Called when the ExtensionActionAPI is shutting down, giving observers a
// chance to unregister themselves if there is not a definitive lifecycle.
void ExtensionActionUtil::OnExtensionActionAPIShuttingDown() {
  extension_action_api_observer_.RemoveAll();
}

void ExtensionActionUtil::OnImageLoaded(const std::string& extension_id,
                                         const gfx::Image& image) {
  if (image.IsEmpty())
    return;

  extensions::api::extension_action_utils::ExtensionInfo info;

  info.id = extension_id;
  info.badge_icon.reset(
      extensions::ExtensionActionUtil::EncodeBitmapToPng(image.ToSkBitmap()));
  scoped_ptr<base::ListValue> args =
      extensions::api::extension_action_utils::OnIconLoaded::Create(info);

  extensions::ExtensionActionUtil::BroadcastEvent(
      extensions::api::extension_action_utils::OnIconLoaded::kEventName,
      args.Pass(), static_cast<content::BrowserContext*>(profile_));
}

/* static */
bool ExtensionActionUtil::FillInfoForTabId(
    api::extension_action_utils::ExtensionInfo& info,
    ExtensionAction* action,
    int tab_id,
    Profile* profile) {

  if (!action) {
    return false;
  }

  info.id = action->extension_id();

  info.badge_tooltip.reset(new std::string(action->GetTitle(tab_id)));

  info.badge_text.reset(new std::string(action->GetBadgeText(tab_id)));

  info.badge_background_color.reset(new std::string(
      SkColorToRGBAString(action->GetBadgeBackgroundColor(tab_id))));

  info.badge_text_color.reset(
      new std::string(SkColorToRGBAString(action->GetBadgeTextColor(tab_id))));

  info.action_type = action->action_type() == ActionInfo::TYPE_BROWSER
                         ? api::extension_action_utils::ACTION_TYPE_BROWSER
                         : api::extension_action_utils::ACTION_TYPE_PAGE;

  info.visible.reset(new bool(action->GetIsVisible(tab_id)));

  info.action_is_hidden.reset(
      new bool(!ExtensionActionAPI::Get(profile)
                   ->GetBrowserActionVisibility(action->extension_id())));

  // Icon precedence:
  //   3. default
  //   2. declarative
  //   1. explicit

  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          action->extension_id());

  extensions::IconImage* defaultIconImage = action->LoadDefaultIconImage(
      *extension, static_cast<content::BrowserContext*>(profile));


  gfx::Image explicitIcon = action->GetExplicitlySetIcon(tab_id);

  gfx::Image declarativeIcon = action->GetDeclarativeIcon(tab_id);

  const SkBitmap* bitmap = nullptr;

  if (!explicitIcon.IsEmpty()) {
    bitmap = explicitIcon.CopySkBitmap();
  } else if (!declarativeIcon.IsEmpty()) {
    bitmap = declarativeIcon.CopySkBitmap();
  } else {
    if (defaultIconImage) {
     bitmap = defaultIconImage->image_skia().bitmap();
    }
  }

  if (bitmap) {
    info.badge_icon.reset(EncodeBitmapToPng(bitmap));
  } else {
    info.badge_icon.reset(new std::string(""));
  }
  return true;
}

/*static*/
std::string* ExtensionActionUtil::EncodeBitmapToPng(const SkBitmap* bitmap) {
  SkAutoLockPixels lock_input(*bitmap);
  unsigned char* inputAddr =
      bitmap->bytesPerPixel() == 1
          ? reinterpret_cast<unsigned char*>(bitmap->getAddr8(0, 0))
          : reinterpret_cast<unsigned char*>(
                bitmap->getAddr32(0, 0));  // bpp = 4

  std::vector<unsigned char> png_data;
  std::vector<gfx::PNGCodec::Comment> comments;
  gfx::PNGCodec::Encode(inputAddr, gfx::PNGCodec::FORMAT_SkBitmap,
                        gfx::Size(bitmap->width(), bitmap->height()),
                        bitmap->rowBytes(), false, comments, &png_data);

  base::StringPiece base64_input(reinterpret_cast<const char*>(&png_data[0]),
                                 png_data.size());
  std::string base64_output;
  Base64Encode(base64_input, &base64_output);

  return new std::string("data:image/png;base64," + base64_output);
}

void ExtensionActionUtil::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {

  api::extension_action_utils::ExtensionInfo info;

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);

  FillInfoForTabId(info, action, ExtensionAction::kDefaultTabId, profile_);

  scoped_ptr<base::ListValue> args =
      api::extension_action_utils::OnRemoved::Create(info);

  BroadcastEvent(api::extension_action_utils::OnRemoved::kEventName,
                 args.Pass(), browser_context);

}

void ExtensionActionUtil::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  api::extension_action_utils::ExtensionInfo info;
  int tab_id = ExtensionAction::kDefaultTabId;

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (!action) {
    return;
  }

  FillInfoForTabId(info, action, tab_id, profile_);

  scoped_ptr<base::ListValue> args =
      api::extension_action_utils::OnAdded::Create(info);

  BroadcastEvent(api::extension_action_utils::OnAdded::kEventName, args.Pass(),
                 browser_context);
  // Lazy-load the default icon
  scoped_ptr<extensions::ExtensionResource> icon_resource;
  icon_resource.reset(new extensions::ExtensionResource(
      extensions::IconsInfo::GetIconResource(
          extension,
          extension_misc::EXTENSION_ICON_BITTY,
          ExtensionIconSet::MATCH_BIGGER)));
  // If there is no default icon for the extension we fall back to the action
  // icon.
  if (icon_resource.get()->extension_root().empty()) {
    base::FilePath icon_path;

    ExtensionAction* pageorbrowser_action = nullptr;

    if (extension->manifest()->HasKey(
            extensions::manifest_keys::kBrowserAction)) {
      pageorbrowser_action = action_manager->GetBrowserAction(*extension);
    } else if (extension->manifest()->HasKey(
                   extensions::manifest_keys::kPageAction)) {
      pageorbrowser_action = action_manager->GetPageAction(*extension);
    }
    std::set<base::FilePath> image_paths;
    if (pageorbrowser_action && pageorbrowser_action->default_icon()) {
      pageorbrowser_action->default_icon()->GetPaths(&image_paths);
    }

    std::set<base::FilePath>::iterator it = image_paths.begin();
    if(it != image_paths.end()) {
      icon_path = *it;
    }

    icon_resource.reset(new extensions::ExtensionResource(
        extension->id(), extension->path(), icon_path));
  }

  if (!icon_resource.get()->extension_root().empty()) {
    extensions::ImageLoader* loader =
        extensions::ImageLoader::Get(browser_context);
    loader->LoadImageAsync(
        extension, *icon_resource.get(),
        gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize),
        base::Bind(&ExtensionActionUtil::OnImageLoaded,
                   weak_ptr_factory_.GetWeakPtr(), extension->id()));
  }

}

void ExtensionActionUtil::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  api::extension_action_utils::ExtensionInfo info;
  info.id = extension->id();
  scoped_ptr<base::ListValue> args =
      api::extension_action_utils::OnRemoved::Create(info);

  BroadcastEvent(api::extension_action_utils::OnRemoved::kEventName,
                 args.Pass(), browser_context);

}

void ExtensionActionUtil::ActiveTabChanged(content::WebContents* old_contents,
                                           content::WebContents* new_contents,
                                           int index,
                                           int reason) {

  // loop through the extensions and update the actions based on the tabid
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile_)->enabled_extensions();

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);

  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const Extension* extension = it->get();

    ExtensionAction* action = action_manager->GetExtensionAction(*extension);

    OnExtensionActionUpdated(action, new_contents,
                             static_cast<content::BrowserContext*>(profile_));

  }

}

ExtensionActionUtilsGetToolbarExtensionsFunction::
    ExtensionActionUtilsGetToolbarExtensionsFunction() {
}

ExtensionActionUtilsGetToolbarExtensionsFunction::
    ~ExtensionActionUtilsGetToolbarExtensionsFunction() {
}

bool ExtensionActionUtilsGetToolbarExtensionsFunction::RunAsync() {
  std::vector<linked_ptr<api::extension_action_utils::ExtensionInfo>>
      toolbar_extensionactions;

  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(GetProfile())->enabled_extensions();

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(GetProfile());

  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const Extension* extension = it->get();

    ExtensionAction* action = action_manager->GetExtensionAction(*extension);

    if (!extension->ShouldNotBeVisible()) {
      linked_ptr<api::extension_action_utils::ExtensionInfo> info(
          new api::extension_action_utils::ExtensionInfo());

      if (ExtensionActionUtil::FillInfoForTabId(*info.get(), action,
                                                ExtensionAction::kDefaultTabId,
                                                GetProfile())) {
        toolbar_extensionactions.push_back(info);
      }
    }
  }

  results_ = api::extension_action_utils::GetToolbarExtensions::Results::Create(
      toolbar_extensionactions);

  SendResponse(true);

  return true;
}

ExtensionActionUtilsExecuteExtensionActionFunction::
    ExtensionActionUtilsExecuteExtensionActionFunction() {
}

ExtensionActionUtilsExecuteExtensionActionFunction::
    ~ExtensionActionUtilsExecuteExtensionActionFunction() {
}

bool ExtensionActionUtilsExecuteExtensionActionFunction::RunAsync() {
  scoped_ptr<api::extension_action_utils::ExecuteExtensionAction::Params>
      params(
          api::extension_action_utils::ExecuteExtensionAction::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(GetProfile());

  const Extension* extension =
      extensions::ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);

  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (!action)
    return false;

  // Note; we cannot use GetAssociatedWebContents since the extension is not
  // running in a tab.
  Browser* browser = nullptr;
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if ((*it)->profile() == GetProfile() &&
        ExtensionTabUtil::GetWindowId((*it)) == *params->window_id.get() &&
        (*it)->window()) {
      browser = (*it);
    }
  }

  api::extension_action_utils::ExtensionInfo info;
  info.id = extension->id();

  if (browser && extensions::ExtensionActionAPI::Get(GetProfile())
          ->ExecuteExtensionAction(extension, browser, true) ==
      ExtensionAction::ACTION_SHOW_POPUP) {
    GURL popup_url = action->GetPopupUrl(SessionTabHelper::IdForTab(
        browser->tab_strip_model()->GetActiveWebContents()));
    info.popup_url.reset(new std::string(popup_url.spec()));
  }

  results_ =
      api::extension_action_utils::ExecuteExtensionAction::Results::Create(
          info);

  SendResponse(true);

  return true;
}

KeyedService* ExtensionActionUtilFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ExtensionActionUtil(static_cast<Profile*>(profile));
}

bool ExtensionActionUtilFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool ExtensionActionUtilFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* ExtensionActionUtilFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}


ExtensionActionUtilsToggleBrowserActionVisibilityFunction::
    ExtensionActionUtilsToggleBrowserActionVisibilityFunction() {
}

ExtensionActionUtilsToggleBrowserActionVisibilityFunction::
    ~ExtensionActionUtilsToggleBrowserActionVisibilityFunction() {
}

bool ExtensionActionUtilsToggleBrowserActionVisibilityFunction::RunAsync() {
  scoped_ptr<ToggleBrowserActionVisibility::Params>
      params(
          ToggleBrowserActionVisibility::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

 extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(GetProfile());

  const Extension* extension =
      extensions::ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);

  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (!action)
    return false;
  ExtensionActionAPI* api = ExtensionActionAPI::Get(GetProfile());
  bool toggled_visibility =
      !api->GetBrowserActionVisibility(params->extension_id);
  api->SetBrowserActionVisibility(params->extension_id, toggled_visibility);
  api->NotifyChange(action, nullptr,
                    static_cast<content::BrowserContext*>(GetProfile()));
  SendResponse(true);
  return true;
}

}  // namespace extensions

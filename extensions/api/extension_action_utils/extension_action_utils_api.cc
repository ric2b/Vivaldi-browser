// Copyright 2015-2019 Vivaldi Technologies AS. All rights reserved.

// This class is just a proxy for emitting events from the Chrome ui for
// browserAction and pageAction badges.
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "app/vivaldi_constants.h"
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/context_menu_helpers.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/common/extensions/api/side_panel/side_panel_info.h"
#include "chrome/grit/theme_resources.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/api/commands/commands_handler.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/tools/vivaldi_tools.h"
#include "extensions/vivaldi_browser_component_wrapper.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_skia_utils.h"

#include "extensions/common/manifest_handlers/permissions_parser.h"

namespace extensions {

namespace {

std::string GetShortcutTextForExtensionAction(
    ExtensionAction* action,
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          action->extension_id(), extensions::ExtensionRegistry::ENABLED);
  if (!extension) {
    return std::string();
  }

  return VivaldiBrowserComponentWrapper::GetInstance()->GetShortcutText(
      browser_context, action);
}

// Encodes the passed bitmap as a PNG represented as a dataurl.
std::string EncodeBitmapToPng(const SkBitmap& bitmap) {

  std::optional<std::vector<uint8_t>> data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false);
  if (!data) {
    return std::string();
  }

  std::string base64_output = base::Base64Encode(data.value());
  base64_output.insert(0, "data:image/png;base64,");

  return base64_output;
}

gfx::ImageSkiaRep ScaleImageSkiaRep(const gfx::ImageSkiaRep& rep,
                                    int target_width_dp,
                                    float target_scale) {
  int width_px = target_width_dp * target_scale;
  return gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(rep.GetBitmap(),
                                    skia::ImageOperations::RESIZE_BEST,
                                    width_px, width_px),
      target_scale);
}

#define USE_HARDCODED_SCALE 1

void FillBitmapForTabId(vivaldi::extension_action_utils::ExtensionInfo* info,
                        ExtensionAction* action,
                        int tab_id) {
  // Icon precedence:
  //   3. default
  //   2. declarative
  //   1. explicit

  extensions::IconImage* default_icon_image = action->default_icon_image();

  gfx::Image explicit_icon = action->GetExplicitlySetIcon(tab_id);
  gfx::Image declarative_icon = action->GetDeclarativeIcon(tab_id);
  gfx::Image image;

  if (!explicit_icon.IsEmpty()) {
    image = explicit_icon;
  } else if (!declarative_icon.IsEmpty()) {
    image = declarative_icon;
  } else if (default_icon_image) {
    image = default_icon_image->image();
  }
  if (!image.IsEmpty()) {
    // Get the image from the extension that matches the DPI we're using
    // on the monitor.
#ifdef USE_HARDCODED_SCALE
    // This matches the previous behavior where we send 32x32 images to JS,
    // which scales them down to 16x16.
    float device_scale = 2.0f;
#else
    Browser* browser = BrowserList::GetInstance()->GetLastActive();
    float device_scale = ui::GetScaleFactorForNativeView(
        browser ? browser->window()->GetNativeWindow() : nullptr);
#endif  // USE_HARDCODED_SCALE

    gfx::ImageSkia skia = image.AsImageSkia();
    gfx::ImageSkiaRep rep = skia.GetRepresentation(device_scale);
    if (rep.scale() != device_scale) {
      skia.AddRepresentation(ScaleImageSkiaRep(
          rep, ExtensionAction::ActionIconSize(), device_scale));
    }
    if (rep.is_null()) {
      info->badge_icon = std::string();
    } else {
      info->badge_icon = EncodeBitmapToPng(rep.GetBitmap());
    }
  } else {
    info->badge_icon = std::string();
  }
}

void UpdateSidePanelInfoIfExists(
    vivaldi::extension_action_utils::ExtensionInfo* info,
    const Extension* extension) {
  const PermissionSet& permissions =
      PermissionsParser::GetRequiredPermissions(extension);

  if (!permissions.HasAPIPermission("sidePanel")) {
    return;
  }

  vivaldi::extension_action_utils::SidePanelInfo info_param;

  auto* side_panel_info =
      static_cast<SidePanelInfo*>(extension->GetManifestData("side_panel"));

  info_param.url = extension->GetResourceURL("").spec();

  if (side_panel_info) {
    GURL url = extension->GetResourceURL(side_panel_info->default_path);
    info_param.active_url = url.spec();
  } else {
    info_param.active_url = "about:blank";
  }
  info->side_panel = std::move(info_param);
}
}  // namespace

void FillInfoFromManifest(vivaldi::extension_action_utils::ExtensionInfo* info,
                          const Extension* extension) {
  info->name = extension->name();

  const std::string* manifest_string =
      extension->manifest()->FindStringPath(manifest_keys::kHomepageURL);
  if (manifest_string) {
    info->homepage = *manifest_string;
  }
  if (OptionsPageInfo::HasOptionsPage(extension)) {
    GURL url = OptionsPageInfo::GetOptionsPage(extension);
    info->optionspage = url.spec();

    bool new_tab = OptionsPageInfo::ShouldOpenInTab(extension);
    info->options_in_new_tab = new_tab;
  }

  UpdateSidePanelInfoIfExists(info, extension);
}

// static
ExtensionActionUtil* ExtensionActionUtilFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ExtensionActionUtil*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
ExtensionActionUtilFactory* ExtensionActionUtilFactory::GetInstance() {
  return base::Singleton<ExtensionActionUtilFactory>::get();
}

ExtensionActionUtilFactory::ExtensionActionUtilFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionActionUtils",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
}

ExtensionActionUtilFactory::~ExtensionActionUtilFactory() {}

std::unique_ptr<KeyedService>
ExtensionActionUtilFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<ExtensionActionUtil>(static_cast<Profile*>(profile));
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
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context);
}

// static
void ExtensionActionUtil::SendIconLoaded(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const gfx::Image& image) {
  if (image.IsEmpty())
    return;

  vivaldi::extension_action_utils::ExtensionInfo info;
  const Extension* extension =
      ExtensionRegistry::Get(browser_context)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  if (!extension) {
    // This has been observed in the wild. VB-83896
    return;
  }
  ExtensionActionManager* manager =
      ExtensionActionManager::Get(browser_context);
  ExtensionAction* action = manager->GetExtensionAction(*extension);

  if (action) {
    FillBitmapForTabId(&info, action, ExtensionAction::kDefaultTabId);
    info.tab_id = ExtensionAction::kDefaultTabId;
    info.id = extension_id;
    // Also include name as it is a mandatory property, otherwise be "" will
    // be used as an update to name.
    info.name = extension->name();

    ::vivaldi::BroadcastEvent(
        vivaldi::extension_action_utils::OnUpdated::kEventName,
        vivaldi::extension_action_utils::OnUpdated::Create(info),
        browser_context);
  }
}

ExtensionActionUtil::ExtensionActionUtil(Profile* profile) : profile_(profile) {
  ExtensionRegistry::Get(profile_)->AddObserver(this);
  VivaldiBrowserComponentWrapper::GetInstance()
      ->AddExtensionActionDispatcherObserver(profile_, this);
}

ExtensionActionUtil::~ExtensionActionUtil() {}

void ExtensionActionUtil::GetExtensionsInfo(
    const ExtensionSet& extensions,
    extensions::ToolbarExtensionInfoList* extension_list) {
  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);

  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const Extension* extension = it->get();

    if (extensions::Manifest::IsComponentLocation(extension->location())) {
      continue;
    }

    vivaldi::extension_action_utils::ExtensionInfo info;

    info.name = extension->name();
    info.id = extension->id();
    info.enabled = registry->enabled_extensions().Contains(info.id);
    info.optionspage = OptionsPageInfo::GetOptionsPage(extension).spec();
    info.homepage = ManifestURL::GetHomepageURL(extension).spec();
    info.tab_id = ExtensionAction::kDefaultTabId;
    info.blocked = registry->blocked_extensions().Contains(info.id);

    // Extensions that has an action needs to be exposed in
    // ExtensionActionToolbar and require all information.
    // However, Quick Commands only require the barebone of extension
    // information, sat above.
    ExtensionAction* action = action_manager->GetExtensionAction(*extension);
    if (action)
      FillInfoForTabId(&info, action, ExtensionAction::kDefaultTabId);

    UpdateSidePanelInfoIfExists(&info, extension);

    extension_list->push_back(std::move(info));
  }
}

void ExtensionActionUtil::FillInfoForTabId(
    vivaldi::extension_action_utils::ExtensionInfo* info,
    ExtensionAction* action,
    int tab_id) {
  info->keyboard_shortcut = GetShortcutTextForExtensionAction(action, profile_);

  info->tab_id = tab_id;

  info->id = action->extension_id();

  // Note, all getters return default values if no explicit value has been set.
  info->badge_tooltip = action->GetTitle(tab_id);

  // If the extension has a non-specific tabId badgetext, used for all tabs.
  info->badge_text = action->GetDisplayBadgeText(tab_id);

  info->badge_background_color =
      color_utils::SkColorToRgbaString(action->GetBadgeBackgroundColor(tab_id));

  info->badge_text_color =
      color_utils::SkColorToRgbaString(action->GetBadgeTextColor(tab_id));

  info->action_type =
      action->action_type() == ActionInfo::Type::kBrowser
                          ? vivaldi::extension_action_utils::ActionType::kBrowser
                          : vivaldi::extension_action_utils::ActionType::kPage;

  info->visible = action->GetIsVisible(tab_id);

  info->allow_in_incognito =
      util::IsIncognitoEnabled(action->extension_id(), profile_);

  FillBitmapForTabId(info, action, tab_id);
}

void ExtensionActionUtil::Shutdown() {
  ExtensionRegistry::Get(profile_)->RemoveObserver(this);
  VivaldiBrowserComponentWrapper::GetInstance()
      ->RemoveExtensionActionDispatcherObserver(profile_, this);
}

void ExtensionActionUtil::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  int tab_id;
  bool is_cleared;
  if (web_contents == nullptr) {
    tab_id = ExtensionAction::kDefaultTabId;
    is_cleared = false;
  } else {
    tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
    // see ExtensionAction::ClearAllValuesForTab()
    is_cleared = !extension_action->HasPopupUrl(tab_id) &&
        !extension_action->HasTitle(tab_id) &&
        !extension_action->HasIcon(tab_id) &&
        !extension_action->HasBadgeText(tab_id) &&
        !extension_action->HasDNRActionCount(tab_id) &&
        !extension_action->HasBadgeTextColor(tab_id) &&
        !extension_action->HasBadgeBackgroundColor(tab_id) &&
        !extension_action->HasIsVisible(tab_id);
  }

  if (is_cleared) {
    ::vivaldi::BroadcastEvent(
        vivaldi::extension_action_utils::OnClearAllValuesForTab::kEventName,
        vivaldi::extension_action_utils::OnClearAllValuesForTab::Create(
            extension_action->extension_id(), tab_id),
        browser_context);
  } else {
    vivaldi::extension_action_utils::ExtensionInfo info;

    info.keyboard_shortcut =
        GetShortcutTextForExtensionAction(extension_action, browser_context);

    // TODO(igor@vivaldi.com): Shall we use the passed browser_context here,
    // not stored profile_? See VB-52519.
    const Extension* extension =
        extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
            extension_action->extension_id(),
            extensions::ExtensionRegistry::ENABLED);
    if (extension) {
      FillInfoFromManifest(&info, extension);
    }


    FillInfoForTabId(&info, extension_action, tab_id);

    ::vivaldi::BroadcastEvent(
        vivaldi::extension_action_utils::OnUpdated::kEventName,
        vivaldi::extension_action_utils::OnUpdated::Create(info),
        browser_context);
  }
}

void ExtensionActionUtil::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  // TODO(igor@vivaldi.com): Shall we use the passed browser_context here,
  // not stored profile_? See See VB-52519.

  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (action) {
    vivaldi::extension_action_utils::ExtensionInfo info;
    FillInfoForTabId(&info, action, ExtensionAction::kDefaultTabId);

    UpdateSidePanelInfoIfExists(&info, extension);

    ::vivaldi::BroadcastEvent(
        vivaldi::extension_action_utils::OnRemoved::kEventName,
        vivaldi::extension_action_utils::OnRemoved::Create(info),
        browser_context);
  }
}

void ExtensionActionUtil::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  // TODO(igor@vivaldi.com): Shall we use the passed browser_context here,
  // not stored profile_? See VB-52519.


  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (!action) {
    return;
  }

  vivaldi::extension_action_utils::ExtensionInfo info;
  int tab_id = ExtensionAction::kDefaultTabId;
  int icon_size = extension_misc::EXTENSION_ICON_MEDIUM;

  FillInfoForTabId(&info, action, tab_id);

  FillInfoFromManifest(&info, extension);

  // Notify the client about the extension info we got so far.
  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnAdded::kEventName,
      vivaldi::extension_action_utils::OnAdded::Create(info), browser_context);

  std::unique_ptr<extensions::ExtensionResource> icon_resource;
  ExtensionAction* pageorbrowser_action =
      action_manager->GetExtensionAction(*extension);

  if (pageorbrowser_action) {
    std::set<base::FilePath> image_paths;
    if (pageorbrowser_action->default_icon()) {
      pageorbrowser_action->default_icon()->GetPaths(&image_paths);
    }

    base::FilePath icon_path;
    std::set<base::FilePath>::iterator it = image_paths.begin();
    if (it != image_paths.end()) {
      // Use the last image path, as it is the biggest.
      icon_path = *(--image_paths.end());
    }
    icon_resource.reset(new extensions::ExtensionResource(
        extension->id(), extension->path(), icon_path));

    icon_size = pageorbrowser_action->default_icon()->GetIconSizeFromPath(
        icon_path.AsUTF8Unsafe());
  }

  // If there are no browser action or page action icons, use the default icons.
  if (!icon_resource.get()) {
    icon_resource.reset(new extensions::ExtensionResource(
        extensions::IconsInfo::GetIconResource(
            extension, extension_misc::EXTENSION_ICON_MEDIUM,
            ExtensionIconSet::Match::kBigger)));
  }

  if (icon_resource.get() && !icon_resource.get()->extension_root().empty()) {
    extensions::ImageLoader* loader =
        extensions::ImageLoader::Get(browser_context);
    loader->LoadImageAsync(extension, *icon_resource.get(),
                           gfx::Size(icon_size, icon_size),
                           base::BindOnce(&ExtensionActionUtil::SendIconLoaded,
                                          browser_context, extension->id()));
  }
}

void ExtensionActionUtil::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  vivaldi::extension_action_utils::ExtensionInfo info;
  info.id = extension->id();

  UpdateSidePanelInfoIfExists(&info, extension);

  ::vivaldi::BroadcastEvent(
      vivaldi::extension_action_utils::OnRemoved::kEventName,
      vivaldi::extension_action_utils::OnRemoved::Create(info),
      browser_context);
}

//////////////////////////////////////////

namespace {

std::string NoSuchExtension(const std::string& extension_id) {
  return "Failed to find an extension with id " + extension_id;
}

std::string NoSuchWindow(int window_id) {
  return "Failed to find a browser window with window_id " +
         std::to_string(window_id);
}

std::string NoExtensionAction(const std::string& extension_id) {
  return "No action for the extension with id " + extension_id;
}

std::string NoSuchMenuItem(const std::string& menu_id) {
  return "No menu action for the menu with id " + menu_id;
}

std::string NoSuchGlobalError(int command_id) {
  return "No error with id " + std::to_string(command_id);
}

}  // namespace

ExtensionFunction::ResponseAction
ExtensionActionUtilsGetToolbarExtensionsFunction::Run() {
  namespace Results =
      vivaldi::extension_action_utils::GetToolbarExtensions::Results;

  extensions::ToolbarExtensionInfoList toolbar_extensionactions;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context());

  ExtensionActionUtil* utilsTest =
      ExtensionActionUtilFactory::GetForBrowserContext(browser_context());

  utilsTest->GetExtensionsInfo(registry->enabled_extensions(),
                               &toolbar_extensionactions);
  utilsTest->GetExtensionsInfo(registry->disabled_extensions(),
                               &toolbar_extensionactions);
  utilsTest->GetExtensionsInfo(registry->terminated_extensions(),
                               &toolbar_extensionactions);
  utilsTest->GetExtensionsInfo(registry->blocked_extensions(),
                               &toolbar_extensionactions);

  return RespondNow(ArgumentList(Results::Create(toolbar_extensionactions)));
}

ExtensionFunction::ResponseAction
ExtensionActionUtilsExecuteExtensionActionFunction::Run() {
  using vivaldi::extension_action_utils::ExtensionActionParams;
  using vivaldi::extension_action_utils::ExecuteExtensionAction::Params;
  namespace Results =
      vivaldi::extension_action_utils::ExecuteExtensionAction::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return RespondNow(Error(NoSuchExtension(params->extension_id)));


  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(browser_context());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  if (!action)
    return RespondNow(Error(NoExtensionAction(params->extension_id)));

  ExtensionActionParams action_params;
  action_params.extension_id = extension->id();

  content::WebContents* web_contents =
      VivaldiBrowserComponentWrapper::GetInstance()->GetActiveWebContents(
          browser_context(),
          params->window_id);

  if (!web_contents)
    return RespondNow(Error(NoSuchWindow(params->window_id)));

  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (action_runner && action_runner->RunAction(extension, true) ==
                           ExtensionAction::ShowAction::kShowPopup) {
    GURL popup_url = action->GetPopupUrl(
        sessions::SessionTabHelper::IdForTab(web_contents).id());
    action_params.popup_url = popup_url.spec();
  }

  auto* side_panel_service =
      extensions::SidePanelService::Get(browser_context());
  if (side_panel_service) {
    action_params.open_panel_on_action_click =
        side_panel_service->OpenSidePanelOnIconClick(params->extension_id);
  }

  return RespondNow(ArgumentList(Results::Create(action_params)));
}

ExtensionFunction::ResponseAction
ExtensionActionUtilsRemoveExtensionFunction::Run() {
  using vivaldi::extension_action_utils::RemoveExtension::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return RespondNow(Error(NoSuchExtension(params->extension_id)));

  Browser* browser =
      VivaldiBrowserComponentWrapper::GetInstance()
          ->FindBrowserByWindowId(params->window_id);
  if (!browser)
    return RespondNow(Error(NoSuchWindow(params->window_id)));

  static_cast<VivaldiBrowserWindow*>(browser->window())
      ->UninstallExtensionViaDialog(extension);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionUtilsShowExtensionOptionsFunction::Run() {
  using vivaldi::extension_action_utils::ShowExtensionOptions::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return RespondNow(Error(NoSuchExtension(params->extension_id)));

  Browser* browser =
      VivaldiBrowserComponentWrapper::GetInstance()
          ->FindBrowserByWindowId(params->window_id);
  if (!browser)
    return RespondNow(Error(NoSuchWindow(params->window_id)));

  DCHECK(OptionsPageInfo::HasOptionsPage(extension));

  VivaldiBrowserComponentWrapper::GetInstance()->OpenExtensionOptionPage(
      extension, browser);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionUtilsShowGlobalErrorFunction::Run() {
  using vivaldi::extension_action_utils::ShowGlobalError::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool success = VivaldiBrowserComponentWrapper::GetInstance()->ShowGlobalError(
      browser_context(), params->command_id, params->window_id);

  if (!success) {
    return RespondNow(Error(NoSuchGlobalError(params->command_id)));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionUtilsTriggerGlobalErrorsFunction::Run() {
  namespace Results =
      vivaldi::extension_action_utils::TriggerGlobalErrors::Results;
  std::vector<vivaldi::extension_action_utils::ExtensionInstallError> jserrors;

  std::vector<ExtensionInstallError*> tmperrors;

  VivaldiBrowserComponentWrapper::GetInstance()->GetGlobalErrors(
      browser_context(), tmperrors);

  // cast back
  for (ExtensionInstallError* error : tmperrors) {
    vivaldi::extension_action_utils::ExtensionInstallError jserror;
    vivaldi::extension_action_utils::ExtensionInstallError* tmp =
    reinterpret_cast<vivaldi::extension_action_utils::ExtensionInstallError*>(
        error);

    jserror.id = tmp->id;
    jserror.name = tmp->name;
    jserror.error_type = tmp->error_type;
    jserror.command_id = tmp->command_id;

    jserrors.push_back(std::move(jserror));
  }

  return RespondNow(ArgumentList(Results::Create(jserrors)));
}

namespace {

vivaldi::extension_action_utils::MenuType MenuItemTypeToEnum(
    MenuItem::Type type) {
  using vivaldi::extension_action_utils::MenuType;

  MenuType type_enum = MenuType::kNone;

  switch (type) {
    case MenuItem::Type::NORMAL:
      type_enum = MenuType::kNormal;
      break;

    case MenuItem::Type::CHECKBOX:
      type_enum = MenuType::kCheckbox;
      break;

    case MenuItem::Type::RADIO:
      type_enum = MenuType::kRadio;
      break;

    case MenuItem::Type::SEPARATOR:
      type_enum = MenuType::kSeparator;
      break;
  }
  return type_enum;
}

std::string GetIDString(const MenuItem::Id& id) {
  return id.uid == 0 ? id.string_uid : base::NumberToString(id.uid);
}

void RecursivelyFillMenu(
    bool top_level,
    const MenuItem::OwnedList* all_items,
    bool can_cross_incognito,
    std::vector<vivaldi::extension_action_utils::MenuItem>& menu_items,
    content::BrowserContext* browser_context) {
  if (!all_items || all_items->empty())
    return;

  const int top_level_limit =
      top_level ? api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT : INT_MAX;
  int cnt = 0;

  for (auto i = all_items->begin();
       i != all_items->end() && cnt < top_level_limit; ++i, cnt++) {
    MenuItem* item = i->get();

    if (item->id().incognito == browser_context->IsOffTheRecord() ||
        can_cross_incognito) {
      vivaldi::extension_action_utils::MenuItem menuitem;

      menuitem.name = item->title();
      menuitem.id = GetIDString(item->id());
      menuitem.visible = item->visible();
      menuitem.enabled = item->enabled();
      menuitem.checked = item->checked();
      menuitem.menu_type = MenuItemTypeToEnum(item->type());

      // Only go down one level from the top as a limit for now.
      if (top_level && item->children().size()) {
        menuitem.submenu = std::vector<vivaldi::extension_action_utils::MenuItem>();
        top_level = false;
        RecursivelyFillMenu(top_level, &item->children(), can_cross_incognito,
                            menuitem.submenu.value(), browser_context);
      }
      menu_items.push_back(std::move(menuitem));
    }
  }
}

std::vector<vivaldi::extension_action_utils::MenuItem> FillMenuFromManifest(
    const Extension* extension,
    content::BrowserContext* browser_context) {
  std::vector<vivaldi::extension_action_utils::MenuItem> menu_items;
  bool can_cross_incognito =
      util::CanCrossIncognito(extension, browser_context);

  const MenuItem::OwnedList* all_items =
      VivaldiBrowserComponentWrapper::GetInstance()->GetExtensionMenuItems(
          browser_context, extension->id());

  RecursivelyFillMenu(true, all_items, can_cross_incognito, menu_items,
                      browser_context);

  return std::vector<vivaldi::extension_action_utils::MenuItem>(
      std::move(menu_items));
}

}  // namespace

ExtensionFunction::ResponseAction
ExtensionActionUtilsGetExtensionMenuFunction::Run() {
  using vivaldi::extension_action_utils::GetExtensionMenu::Params;
  namespace Results =
      vivaldi::extension_action_utils::GetExtensionMenu::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension) {
    return RespondNow(Error(NoSuchExtension(params->extension_id)));
  }

  const std::vector<vivaldi::extension_action_utils::MenuItem> menu =
      FillMenuFromManifest(extension, browser_context());

  return RespondNow(ArgumentList(Results::Create(menu)));
}

ExtensionFunction::ResponseAction
ExtensionActionUtilsExecuteMenuActionFunction::Run() {
  using vivaldi::extension_action_utils::ExecuteMenuAction::Params;
  namespace Results =
      vivaldi::extension_action_utils::ExecuteMenuAction::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!VivaldiBrowserComponentWrapper::GetInstance()->ExecuteCommandMenuItem(
      browser_context(), params->extension_id,params->window_id, params->menu_id)) {
    return RespondNow(Error(NoSuchMenuItem(params->menu_id)));
  }
  return RespondNow(ArgumentList(Results::Create(true)));
}


}  // namespace extensions

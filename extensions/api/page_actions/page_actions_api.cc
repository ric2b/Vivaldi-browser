// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/page_actions/page_actions_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "components/page_actions/page_actions_service_factory.h"
#include "components/page_actions/page_actions_tab_helper.h"
#include "extensions/schema/page_actions.h"
#include "extensions/tools/vivaldi_tools.h"

namespace extensions {

namespace {
std::optional<page_actions::Service::ScriptOverride>
FromVivaldiPageActionsScriptOverride(
    vivaldi::page_actions::ScriptOverride script_override) {
  switch (script_override) {
    case vivaldi::page_actions::ScriptOverride::kNoOverride:
      return page_actions::Service::kNoOverride;
    case vivaldi::page_actions::ScriptOverride::kEnabled:
      return page_actions::Service::kEnabledOverride;
    case vivaldi::page_actions::ScriptOverride::kDisabled:
      return page_actions::Service::kDisabledOverride;
    default:
      NOTREACHED();
  }
}
}  // namespace

PageActionsEventRouter::PageActionsEventRouter(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  auto* service =
      page_actions::ServiceFactory::GetForBrowserContext(browser_context_);
  service->AddObserver(this);
}

void PageActionsEventRouter::OnScriptPathsChanged() {
  ::vivaldi::BroadcastEvent(vivaldi::page_actions::OnScriptsChanged::kEventName,
                            vivaldi::page_actions::OnScriptsChanged::Create(),
                            browser_context_);
}

void PageActionsEventRouter::OnScriptOverridesChanged(
    content::WebContents* tab_contents,
    const base::FilePath& script_path,
    page_actions::Service::ScriptOverride script_override) {
  ::vivaldi::BroadcastEvent(
      vivaldi::page_actions::OnOverridesChanged::kEventName,
      vivaldi::page_actions::OnOverridesChanged::Create(
          ExtensionTabUtil::GetTabId(tab_contents)),
      browser_context_);
}

PageActionsEventRouter::~PageActionsEventRouter() {
  auto* service = page_actions::ServiceFactory::GetForBrowserContextIfExists(
      browser_context_);
  if (service)
    service->RemoveObserver(this);
}

PageActionsAPI::PageActionsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, vivaldi::page_actions::OnScriptsChanged::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::page_actions::OnOverridesChanged::kEventName);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<PageActionsAPI>>::
    DestructorAtExit g_factory_page_actions = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<PageActionsAPI>*
PageActionsAPI::GetFactoryInstance() {
  return g_factory_page_actions.Pointer();
}

PageActionsAPI::~PageActionsAPI() {}

void PageActionsAPI::OnListenerAdded(const EventListenerInfo& details) {
  page_actions_event_router_.reset(
      new PageActionsEventRouter(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

// KeyedService implementation.
void PageActionsAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  page_actions_event_router_.reset();
}

ExtensionFunction::ResponseAction PageActionsFunction::Run() {
  page_actions::Service* service =
      page_actions::ServiceFactory::GetForBrowserContext(browser_context());

  if (!service->IsLoaded()) {
    service->AddObserver(this);
    // Keep the function alive until we can respond.
    AddRef();
    return RespondLater();
  }

  return RespondNow(RunWithService(service));
}

void PageActionsFunction::OnServiceLoaded(page_actions::Service* service) {
  service->RemoveObserver(this);
  Respond(RunWithService(service));
  // Balance AddRef in Run().
  Release();
}

/*static*/
ExtensionFunction::ResponseValue PageActionsFunction::ValidationFailure(
    PageActionsFunction* function) {
  return function->BadMessage();
}

ExtensionFunction::ResponseValue PageActionsGetScriptsFunction::RunWithService(
    page_actions::Service* service) {
  namespace Results = vivaldi::page_actions::GetScripts::Results;

  std::vector<base::FilePath> paths = service->GetAllScriptPaths();

  std::vector<std::string> response;
  std::transform(paths.begin(), paths.end(), std::back_inserter(response),
                 [](base::FilePath p) { return p.AsUTF8Unsafe(); });

  return ArgumentList(Results::Create(response));
}

ExtensionFunction::ResponseValue
PageActionsSetScriptOverrideForTabFunction::RunWithService(
    page_actions::Service* service) {
  namespace Results = vivaldi::page_actions::SetScriptOverrideForTab::Results;
  using vivaldi::page_actions::SetScriptOverrideForTab::Params;
  std::optional<Params> params(Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  bool result = false;
  content::WebContents* tab_contents;
  if (ExtensionTabUtil::GetTabById(params->tab_id, browser_context(), true,
                                   &tab_contents) &&
      tab_contents) {
    result = service->SetScriptOverrideForTab(
        tab_contents, base::FilePath::FromUTF8Unsafe(params->script),
        FromVivaldiPageActionsScriptOverride(params->override).value());
  }

  return ArgumentList(Results::Create(result));
}

ExtensionFunction::ResponseValue
PageActionsGetScriptOverridesForTabFunction::RunWithService(
    page_actions::Service* service) {
  namespace Results = vivaldi::page_actions::GetScriptOverridesForTab::Results;
  using vivaldi::page_actions::GetScriptOverridesForTab::Params;
  std::optional<Params> params(Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<vivaldi::page_actions::OverridenScript> result;
  content::WebContents* tab_contents;
  if (ExtensionTabUtil::GetTabById(params->tab_id, browser_context(), true,
                                   &tab_contents) &&
      tab_contents) {
    auto* page_actions_helper =
        page_actions::TabHelper::FromWebContents(tab_contents);
    if (page_actions_helper) {
      for (const auto& script_override :
           page_actions_helper->GetScriptOverrides()) {
        vivaldi::page_actions::OverridenScript overriden_script;
        overriden_script.script = script_override.first.AsUTF8Unsafe();
        overriden_script.override =
            script_override.second
                ? vivaldi::page_actions::ScriptOverride::kEnabled
                : vivaldi::page_actions::ScriptOverride::kDisabled;
        result.push_back(std::move(overriden_script));
      }
    }
  }

  return ArgumentList(Results::Create(result));
}
}  // namespace extensions

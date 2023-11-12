// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_PAGE_ACTIONS_PAGE_ACTIONS_API_H_
#define EXTENSIONS_API_PAGE_ACTIONS_PAGE_ACTIONS_API_H_

#include <memory>

#include "components/page_actions/page_actions_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Observes the page actions service and then routes the notifications as
// events to the extension system.
class PageActionsEventRouter : page_actions::Service::Observer {
 public:
  explicit PageActionsEventRouter(content::BrowserContext* browser_context);
  PageActionsEventRouter(const PageActionsEventRouter& other) = delete;
  PageActionsEventRouter& operator=(const PageActionsEventRouter& other) =
      delete;
  ~PageActionsEventRouter() override;

  // page_actions::Service::Observer implementation.
  void OnScriptPathsChanged() override;
  void OnScriptOverridesChanged(
      content::WebContents* tab_contents,
      const base::FilePath& script_path,
      page_actions::Service::ScriptOverride script_override) override;

 private:
  const raw_ptr<content::BrowserContext> browser_context_;
};

class PageActionsAPI : public BrowserContextKeyedAPI,
                       public EventRouter::Observer {
 public:
  explicit PageActionsAPI(content::BrowserContext* context);
  PageActionsAPI(const PageActionsAPI& other) = delete;
  PageActionsAPI& operator=(const PageActionsAPI& other) = delete;
  ~PageActionsAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PageActionsAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<PageActionsAPI>;

  const raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "pageActionsAPI"; }

  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = false;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<PageActionsEventRouter> page_actions_event_router_;
};

class PageActionsFunction : public ExtensionFunction,
                            public page_actions::Service::Observer {
 public:
  PageActionsFunction() = default;
  PageActionsFunction(const PageActionsFunction& other) = delete;
  PageActionsFunction& operator=(const PageActionsFunction& other) = delete;

  //  page_actions::Service::Observer implementation.
  void OnServiceLoaded(page_actions::Service* service) override;

 protected:
  ~PageActionsFunction() override = default;
  virtual ResponseValue RunWithService(page_actions::Service* service) = 0;
  static ResponseValue ValidationFailure(PageActionsFunction* function);

 private:
  ResponseAction Run() override;
};

class PageActionsGetScriptsFunction : public PageActionsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageActions.getScripts", PAGE_ACTIONS_GET_SCRIPTS)
  PageActionsGetScriptsFunction() = default;
  PageActionsGetScriptsFunction(const PageActionsGetScriptsFunction& other) =
      delete;
  PageActionsGetScriptsFunction& operator=(
      const PageActionsGetScriptsFunction& other) = delete;

 private:
  ~PageActionsGetScriptsFunction() override = default;
  // PageActionsFunction:
  ResponseValue RunWithService(page_actions::Service* service) override;
};

class PageActionsSetScriptOverrideForTabFunction : public PageActionsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageActions.setScriptOverrideForTab",
                             PAGE_ACTIONS_SET_SCRIPT_OVERRIDE_FOR_TAB)
  PageActionsSetScriptOverrideForTabFunction() = default;
  PageActionsSetScriptOverrideForTabFunction(
      const PageActionsSetScriptOverrideForTabFunction& other) = delete;
  PageActionsSetScriptOverrideForTabFunction& operator=(
      const PageActionsSetScriptOverrideForTabFunction& other) = delete;

 private:
  ~PageActionsSetScriptOverrideForTabFunction() override = default;
  // PageActionsFunction:
  ResponseValue RunWithService(page_actions::Service* service) override;
};

class PageActionsGetScriptOverridesForTabFunction : public PageActionsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageActions.getScriptOverridesForTab",
                             PAGE_ACTIONS_GET_SCRIPT_OVERRIDES_FOR_TAB)
  PageActionsGetScriptOverridesForTabFunction() = default;
  PageActionsGetScriptOverridesForTabFunction(
      const PageActionsGetScriptOverridesForTabFunction& other) = delete;
  PageActionsGetScriptOverridesForTabFunction& operator=(
      const PageActionsGetScriptOverridesForTabFunction& other) = delete;

 private:
  ~PageActionsGetScriptOverridesForTabFunction() override = default;
  // PageActionsFunction:
  ResponseValue RunWithService(page_actions::Service* service) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_PAGE_ACTIONS_PAGE_ACTIONS_API_H_

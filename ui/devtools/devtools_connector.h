// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UI_DEVTOOLS_DEVTOOLS_CONNECTOR_H_
#define UI_DEVTOOLS_DEVTOOLS_CONNECTOR_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "net/cert/x509_certificate.h"

class Browser;

namespace content {
class BrowserContext;
}

namespace extensions {

class UIBindingsDelegate : public DevToolsUIBindings::Delegate {
 public:
  UIBindingsDelegate(content::BrowserContext* browser_context,
                     int tab_id,
                     DevToolsUIBindings::Delegate* delegate);
  ~UIBindingsDelegate() override;

  int tab_id() { return tab_id_; }

  // DevToolsUIBindings::Delegate implementation
  void ActivateWindow() override;
  void CloseWindow() override;
  void Inspect(scoped_refptr<content::DevToolsAgentHost> host) override;
  void SetInspectedPageBounds(const gfx::Rect& rect) override;
  void InspectElementCompleted() override;
  void SetIsDocked(bool is_docked) override;
  void OpenInNewTab(const std::string& url) override;
  void OpenSearchResultsInNewTab(const std::string& query) override;
  void SetWhitelistedShortcuts(const std::string& message) override;
  void InspectedContentsClosing() override;
  void OnLoadCompleted() override;
  void OpenNodeFrontend() override;
  void ReadyForTest() override;
  infobars::ContentInfoBarManager* GetInfoBarManager() override;
  void RenderProcessGone(bool crashed) override;
  void SetEyeDropperActive(bool active) override;
  void ShowCertificateViewer(const std::string& cert_chain) override;
  void ConnectionReady() override;
  void SetOpenNewWindowForPopups(bool value) override;

  int GetDockStateForLogging() override;
  int GetOpenedByForLogging() override;
  int GetClosedByForLogging() override;

 private:
  // Notify JS side to update bounds.
  void NotifyUpdateBounds();

  // Original delegate owned by us.
  std::unique_ptr<DevToolsUIBindings::Delegate> ui_bindings_delegate_;

  int tab_id_ = 0;
  content::BrowserContext* browser_context_ = nullptr;
};

class DevtoolsConnectorItem : public content::WebContentsDelegate {
 public:
  DevtoolsConnectorItem() = default;
  DevtoolsConnectorItem(int tab_id, content::BrowserContext* context);
  ~DevtoolsConnectorItem() override;

  void set_devtools_delegate(content::WebContentsDelegate* delegate) {
    devtools_delegate_ = delegate;
  }

  void set_ui_bindings_delegate(DevToolsUIBindings::Delegate* delegate) {
    ui_bindings_bindings_delegate_ =
        new UIBindingsDelegate(browser_context_, tab_id(), delegate);
  }

  const content::WebContentsDelegate* devtoools_delegate() {
    return devtools_delegate_;
  }

  DevToolsUIBindings::Delegate* ui_bindings_delegate() {
    return ui_bindings_bindings_delegate_;
  }

  int tab_id() { return tab_id_; }

  void ResetDockingState();

  std::string docking_state() { return devtools_docking_state_; }
  void set_docking_state(std::string& docking_state) {
    devtools_docking_state_ = docking_state;
  }

  bool device_mode_enabled() { return device_mode_enabled_; }
  void set_device_mode_enabled(bool enabled) { device_mode_enabled_ = enabled; }

 private:
  friend class base::RefCounted<DevtoolsConnectorItem>;

  // content::WebContentsDelegate:
  void ActivateContents(content::WebContents* contents) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture,
                      bool* was_blocked) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void CloseContents(content::WebContents* source) override;
  void ContentsZoomChange(bool zoom_in) override;
  void BeforeUnloadFired(content::WebContents* tab,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)> navigation_handle_callback)
      override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;

  // These are the original delegates Chromium would normally use
  // and we call into them to allow existing functionality to work.
  content::WebContentsDelegate* devtools_delegate_ = nullptr;

  int tab_id_ = 0;
  content::BrowserContext* browser_context_ = nullptr;

  // |ui_bindings_bindings_delegate_| is owned by DevToolsUIBindings.
  UIBindingsDelegate* ui_bindings_bindings_delegate_ = nullptr;

  // Keeps track of the docking state per tab.
  std::string devtools_docking_state_ = "off";

  // Keeps track of the device mode state
  bool device_mode_enabled_ = false;
};

/*
This class controls the bridge delegates between the webview and the
DevtoolsWindow. Both classes needs to be set as a WebContentsDelegate,
so to handle that we assign that delegate using this class that will
delegate to both.
*/
class DevtoolsConnectorAPI : public BrowserContextKeyedAPI {
 public:
  explicit DevtoolsConnectorAPI(content::BrowserContext* context);
  ~DevtoolsConnectorAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<DevtoolsConnectorAPI>*
  GetFactoryInstance();

  DevtoolsConnectorItem* GetOrCreateDevtoolsConnectorItem(int tab_id);

  void RemoveDevtoolsConnectorItem(int tab_id);
  static void CloseAllDevtools();

  // Browser or nullptr to close all open devtools.
  static void CloseDevtoolsForBrowser(content::BrowserContext* browser_context,
                                      Browser* browser);

  static void SendOnUndockedEvent(content::BrowserContext* context,
                                  int tab_id,
                                  bool show_window);

  static void SendDockingStateChanged(content::BrowserContext* browser_context,
                                      int tab_id,
                                      const std::string& docking_state);

  static void SendClosed(content::BrowserContext* browser_context, int tab_id);

 private:
  friend class BrowserContextKeyedAPIFactory<DevtoolsConnectorAPI>;

  content::BrowserContext* browser_context_;

  // The guest view has ownership of the pointers contained within.
  std::vector<DevtoolsConnectorItem*> connector_items_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "DevtoolsConnectorAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;
};

}  // namespace extensions

#endif  // UI_DEVTOOLS_DEVTOOLS_CONNECTOR_H_

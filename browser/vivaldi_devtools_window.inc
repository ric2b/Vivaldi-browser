// include in "chrome/browser/devtools/devtools_window.cc"

// static
content::WebContents* DevToolsWindow::GetDevtoolsWebContentsForInspectedWebContents(
    content::WebContents* inspected_web_contents) {
  if (!inspected_web_contents || !g_devtools_window_instances.IsCreated())
    return nullptr;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->GetInspectedWebContents() == inspected_web_contents)
      return (*it)->main_web_contents_;
  }
  return nullptr;
}

// static
DevToolsWindow* DevToolsWindow::FindWindowByDevtoolsWebContents(
    content::WebContents* devtools_web_contents) {
  if (!devtools_web_contents || !g_devtools_window_instances.IsCreated())
    return nullptr;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (DevToolsWindow* window : *instances) {
    if (window->main_web_contents_ == devtools_web_contents) {
      return window;
    }
  }
  return nullptr;
}

bool DevToolsWindow::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  if (guest_delegate_) {
    return guest_delegate_->HandleContextMenu(render_frame_host, params);
  }
  return false;
}

void DevToolsWindow::ForceCloseWindow() {
  // Closing all tabs in a undocked window does the proper cleanup.
  if (browser_) {
    browser_->OnWindowClosing();
  } else {
    CloseContents(main_web_contents_);
  }
}

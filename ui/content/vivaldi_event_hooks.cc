// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "ui/content/vivaldi_event_hooks.h"

#include "app/vivaldi_apptools.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"

// static
const void* VivaldiEventHooks::UserDataKey() {
  static const int kUserDataKey = 0;
  return &kUserDataKey;
}

// static
VivaldiEventHooks* VivaldiEventHooks::FromOutermostContents(
    content::WebContents* web_contents) {
  DCHECK(::vivaldi::IsVivaldiRunning());
  DCHECK(!web_contents->GetOuterWebContents());
  // data is null for DevTools
  base::SupportsUserData::Data* data = web_contents->GetUserData(UserDataKey());
  return static_cast<VivaldiEventHooks*>(data);
}

// static
VivaldiEventHooks* VivaldiEventHooks::FromRootView(
    content::RenderWidgetHostViewBase* root_view) {
  // Follow Chromium pattern for ::From methods and allow a null argument.
  if (!root_view)
    return nullptr;
  DCHECK(root_view == root_view->GetRootView());

  // For a root view WebContents is the outermost. So to skip a rather expensive
  // call to WebContens::GetOutermostWebContents() inline FromRenderWidgetHost.
  content::RenderWidgetHostImpl* widget_host = root_view->host();
  if (!widget_host)
    return nullptr;
  content::WebContents* web_contents =
      widget_host->delegate()->GetAsWebContents();
  if (!web_contents)
    return nullptr;
  return FromOutermostContents(web_contents);
}

// static
VivaldiEventHooks* VivaldiEventHooks::FromRenderWidgetHost(
    content::RenderWidgetHostImpl* widget_host) {
  if (!widget_host)
    return nullptr;
  return FromWebContents(widget_host->delegate()->GetAsWebContents());
}

// static
VivaldiEventHooks* VivaldiEventHooks::FromWebContents(
    content::WebContents* web_contents) {
  // Follow Chromium pattern for ::From methods and allow a null argument.
  if (!web_contents)
    return nullptr;
  return FromOutermostContents(web_contents->GetOutermostWebContents());
}

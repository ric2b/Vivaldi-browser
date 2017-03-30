// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "renderer/vivaldi_render_view_observer.h"

#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace vivaldi {

VivaldiRenderViewObserver::VivaldiRenderViewObserver(
  content::RenderView* render_view)
: content::RenderViewObserver(render_view)
{}

VivaldiRenderViewObserver::~VivaldiRenderViewObserver() {
}

void VivaldiRenderViewObserver::OnDestruct() {
  delete this;
}

bool VivaldiRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VivaldiRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(VivaldiMsg_InsertText, OnInsertText)
    IPC_MESSAGE_HANDLER(VivaldiMsg_SetPinchZoom, OnPinchZoom)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Inserts text into input fields.
void VivaldiRenderViewObserver::OnInsertText(const base::string16& text) {
  blink::WebLocalFrame* frame = render_view()->GetWebView()->focusedFrame();
  frame->insertText(text);
}

void VivaldiRenderViewObserver::OnPinchZoom(float scale, int x, int y) {
  render_view()->GetWebView()->vivaldiSetPinchZoom(scale, x, y);
}

}  // namespace vivaldi

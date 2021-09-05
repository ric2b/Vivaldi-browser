// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "renderer/vivaldi_media_element_event_sender.h"

#include <memory>
#include "content/public/renderer/render_thread.h"
#include "renderer/vivaldi_render_messages.h"

VivaldiMediaElementEventSender::VivaldiMediaElementEventSender() {}
VivaldiMediaElementEventSender::~VivaldiMediaElementEventSender() {}

// static
std::unique_ptr<VivaldiMediaElementEventDelegate>
VivaldiMediaElementEventDelegate::CreateVivaldiMediaElementEventSender() {
  return std::make_unique<VivaldiMediaElementEventSender>();
}


void VivaldiMediaElementEventSender::SendAddedEventToBrowser() {
  content::RenderThread::Get()->Send(new VivaldiMsg_MediaElementAddedEvent);
}

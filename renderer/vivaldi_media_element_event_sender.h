// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_VIVALDI_MEDIA_ELEMENT_EVENT_SENDER_H_
#define RENDERER_VIVALDI_MEDIA_ELEMENT_EVENT_SENDER_H_

#include "renderer/vivaldi_media_element_event_delegate.h"

namespace blink {
class Event;
}

// MediaElementEventSender is responsible for sending video
// element events to the browser process to be used for Picture-In-Picture.
class VivaldiMediaElementEventSender
    : public VivaldiMediaElementEventDelegate {
 public:
  VivaldiMediaElementEventSender();
  ~VivaldiMediaElementEventSender() override;

  void SendAddedEventToBrowser() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiMediaElementEventSender);
};

#endif  // RENDERER_VIVALDI_MEDIA_ELEMENT_EVENT_SENDER_H_

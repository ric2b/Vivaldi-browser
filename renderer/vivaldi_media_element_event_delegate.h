// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_VIVALDI_MEDIA_ELEMENT_EVENT_DELEGATE_H_
#define RENDERER_VIVALDI_MEDIA_ELEMENT_EVENT_DELEGATE_H_

#include <memory>
#include "base/macros.h"

namespace blink {
class Event;
}

class VivaldiMediaElementEventDelegate {
 public:
  VivaldiMediaElementEventDelegate() = default;
  virtual ~VivaldiMediaElementEventDelegate() = default;

  virtual void SendAddedEventToBrowser() = 0;

  static std::unique_ptr<VivaldiMediaElementEventDelegate>
  CreateVivaldiMediaElementEventSender();

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiMediaElementEventDelegate);
};

#endif  // RENDERER_VIVALDI_MEDIA_ELEMENT_EVENT_DELEGATE_H_

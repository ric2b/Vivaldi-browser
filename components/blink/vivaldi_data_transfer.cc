// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"

#include "third_party/blink/renderer/core/clipboard/data_transfer.h"

namespace blink {

void DataTransfer::SetURLAndTitle(const String& url, const String& title) {
  if (!CanWriteData())
    return;

  data_object_->SetURLAndTitle(url, title);
}

}  // namespace blink

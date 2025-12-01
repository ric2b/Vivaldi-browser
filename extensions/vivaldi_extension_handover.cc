// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved.

#include "extensions/vivaldi_extension_handover.h"

namespace vivaldi {

static VivaldiExtensionHandover* handover_impl_ = nullptr;

VivaldiExtensionHandover* VivaldiExtensionHandover::GetInstance() {
  return handover_impl_;
}

void VivaldiExtensionHandover::SetInstance(VivaldiExtensionHandover* instance) {
  handover_impl_ = instance;
}

}  // namespace vivaldi

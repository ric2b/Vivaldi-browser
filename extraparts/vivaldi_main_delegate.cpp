// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_main_delegate.h"

#include "extensions/vivaldi_extensions_client.h"

VivaldiMainDelegate::VivaldiMainDelegate() {
  extensions::VivaldiExtensionsClient::RegisterVivaldiExtensionsClient();
}

VivaldiMainDelegate::~VivaldiMainDelegate() {}

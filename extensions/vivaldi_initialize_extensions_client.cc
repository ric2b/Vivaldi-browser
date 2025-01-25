// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/initialize_extensions_client.h"

#include "base/no_destructor.h"
#include "chrome/common/apps/platform_apps/chrome_apps_api_provider.h"
#include "chrome/common/controlled_frame/controlled_frame_api_provider.h"
#include "extensions/common/extensions_client.h"
#include "extensions/vivaldi_extensions_client.h"

void EnsureExtensionsClientInitialized(
  extensions::Feature::FeatureDelegatedAvailabilityCheckMap
  delegated_availability_map) {
  static bool initialized = false;

  static base::NoDestructor<extensions::VivaldiExtensionsClient>
    extensions_client;

  if (!initialized) {
    initialized = true;
    extensions_client->SetFeatureDelegatedAvailabilityCheckMap(
      std::move(delegated_availability_map));
    //extensions_client->AddAPIProvider(
    //  std::make_unique<chrome_apps::ChromeAppsAPIProvider>());
    extensions_client->AddAPIProvider(
      std::make_unique<controlled_frame::ControlledFrameAPIProvider>());
    extensions::ExtensionsClient::Set(extensions_client.get());
  }

  // ExtensionsClient::Set() will early-out if the client was already set, so
  // this allows us to check that this was the only site setting it.
  DCHECK_EQ(extensions_client.get(), extensions::ExtensionsClient::Get())
    << "ExtensionsClient should only be initialized through "
    << "EnsureExtensionsClientInitialized() when using "
    << "ChromeExtensionsClient.";
}

void EnsureExtensionsClientInitialized() {
  extensions::Feature::FeatureDelegatedAvailabilityCheckMap map;
  EnsureExtensionsClientInitialized(std::move(map));

  /*  static bool initialized = false;

    static base::NoDestructor<extensions::VivaldiExtensionsClient>
        extensions_client;

    if (!initialized) {
      initialized = true;
      extensions_client->AddAPIProvider(
        std::make_unique<chrome_apps::ChromeAppsAPIProvider>());
      extensions_client->AddAPIProvider(
        std::make_unique<controlled_frame::ControlledFrameAPIProvider>());
      extensions::ExtensionsClient::Set(extensions_client.get());
    }

    // ExtensionsClient::Set() will early-out if the client was already set, so
    // this allows us to check that this was the only site setting it.
    DCHECK_EQ(extensions_client.get(), extensions::ExtensionsClient::Get())
        << "ExtensionsClient should only be initialized through "
        << "EnsureExtensionsClientInitialized() when using "
        << "ChromeExtensionsClient.";*/
}

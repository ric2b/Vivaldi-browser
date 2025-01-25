// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "components/os_crypt/key_storage_xdg_desktop_portal.h"

#include <optional>
#include <utility>

#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"

#include "components/os_crypt/xdg_desktop_portal_dbus.h"

KeyStoragePortal::KeyStoragePortal() {}

KeyStoragePortal::~KeyStoragePortal() = default;

bool KeyStoragePortal::Init() {
  portal_dbus_ = std::make_unique<SecretPortalDBus>();
  return true;
}

std::optional<std::string> KeyStoragePortal::GetKeyImpl() {
  std::string result;

  if (!portal_dbus_->RetrieveSecret(&result)) {
    LOG(ERROR) << "Could not retrieve portal secret from xdg-desktop-portal";
    return {};
  }

  return {result};
}

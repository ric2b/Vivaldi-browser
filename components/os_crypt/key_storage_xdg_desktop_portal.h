// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_OS_CRYPT_KEY_STORAGE_PORTAL_H_
#define COMPONENTS_OS_CRYPT_KEY_STORAGE_PORTAL_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "components/os_crypt/sync/key_storage_linux.h"

// forward of the implementation
class SecretPortalDBus;

// Specialisation of KeyStorageLinux that uses xdg-desktop-portal.
class COMPONENT_EXPORT(OS_CRYPT) KeyStoragePortal : public KeyStorageLinux {
 public:
  explicit KeyStoragePortal();

  KeyStoragePortal(const KeyStoragePortal&) = delete;
  KeyStoragePortal& operator=(const KeyStoragePortal&) = delete;

  ~KeyStoragePortal() override;

 protected:
  // KeyStorageLinux
  bool Init() override;
  std::optional<std::string> GetKeyImpl() override;

 private:
  std::unique_ptr<SecretPortalDBus> portal_dbus_;
};

#endif  // COMPONENTS_OS_CRYPT_KEY_STORAGE_PORTAL_H_

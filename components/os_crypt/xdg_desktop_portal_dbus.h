// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_OS_CRYPT_XDG_PORTAL_DBUS_H
#define COMPONENTS_OS_CRYPT_XDG_PORTAL_DBUS_H

#include <csignal>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class RunLoop;
class File;
class RunLoop;
}  // namespace base

namespace dbus {
class Bus;
class ObjectProxy;
class Signal;
}  // namespace dbus

// Contains wrappers for dbus invocations related to XDG Desktop Portal Secret.
class COMPONENT_EXPORT(OS_CRYPT) SecretPortalDBus {
 public:
  SecretPortalDBus();
  ~SecretPortalDBus();

  bool RetrieveSecret(std::optional<std::string> token, std::string* output);

  bool RetrieveSecret(std::string* output);

 private:
  void OnResponse(dbus::Signal* signal);
  void OnConnected(const std::string& interface,
                   const std::string& signal,
                   bool success);

  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<base::File> secret_reader_;
  std::string secret_;
};

#endif /* COMPONENTS_OS_CRYPT_XDG_PORTAL_DBUS_H */

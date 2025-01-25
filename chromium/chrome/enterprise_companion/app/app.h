// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_APP_APP_H_
#define CHROME_ENTERPRISE_COMPANION_APP_APP_H_

#include <memory>

#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/installer.h"
#include "chrome/enterprise_companion/lock.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

namespace enterprise_companion {

class App {
 public:
  App();
  virtual ~App();

  // Runs the App, blocking until completion.
  [[nodiscard]] EnterpriseCompanionStatus Run();

 protected:
  // Concrete implementations of App can execute their first task in this
  // method. It is called on the main sequence. It may call Shutdown.
  virtual void FirstTaskRun() = 0;

  // Triggers app shutdown. Must be called on the main sequence.
  void Shutdown(const EnterpriseCompanionStatus& status);

 private:
  // A callback that allows `Run` to complete.
  StatusCallback quit_;
};

// Creates an App which runs the EnterpriseCompanion IPC server process.
std::unique_ptr<App> CreateAppServer();

// Creates an App which instructs the running server to exit, if present.
std::unique_ptr<App> CreateAppShutdown(
    const mojo::NamedPlatformChannel::ServerName& server_name =
        GetServerName());

std::unique_ptr<App> CreateAppInstall(
    base::OnceCallback<EnterpriseCompanionStatus()> shutdown_remote_task =
        base::BindOnce([] { return CreateAppShutdown()->Run(); }),
    base::OnceCallback<std::unique_ptr<ScopedLock>(base::TimeDelta timeout)>
        lock_provider = base::BindOnce(&CreateScopedLock),
    base::OnceCallback<bool()> install_task = base::BindOnce(&Install));

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_APP_APP_H_

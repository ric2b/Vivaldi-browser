//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_
#define EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/schema/vivaldi_sessions.h"

namespace sessions {
class SessionCommand;
}

namespace extensions {

class SessionsPrivateSaveOpenTabsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.saveOpenTabs",
                             SESSIONS_SAVEOPENTABS)
  SessionsPrivateSaveOpenTabsFunction() = default;

 private:
  ~SessionsPrivateSaveOpenTabsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateSaveOpenTabsFunction);
};

class SessionsPrivateGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.getAll", SESSIONS_GETALL)
  SessionsPrivateGetAllFunction() = default;

 private:
  ~SessionsPrivateGetAllFunction() override = default;

  struct SessionEntry {
    ~SessionEntry();
    SessionEntry();
    std::unique_ptr<extensions::vivaldi::sessions_private::SessionItem> item;
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands;

    DISALLOW_COPY_AND_ASSIGN(SessionEntry);
  };

  std::vector<std::unique_ptr<SessionsPrivateGetAllFunction::SessionEntry>>
  RunOnFileThread(base::FilePath path);
  void SendResponse(std::vector<std::unique_ptr<SessionEntry>> sessions);

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateGetAllFunction);
};

class SessionsPrivateOpenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.open", SESSIONS_OPEN)
  SessionsPrivateOpenFunction() = default;

 private:
  ~SessionsPrivateOpenFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateOpenFunction);
};

class SessionsPrivateDeleteFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.delete", SESSIONS_DELETE)
  SessionsPrivateDeleteFunction() = default;

 private:
  ~SessionsPrivateDeleteFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateDeleteFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_

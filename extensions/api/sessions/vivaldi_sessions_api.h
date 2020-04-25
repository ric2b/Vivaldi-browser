//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_
#define EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/schema/vivaldi_sessions.h"

namespace extensions {

class SessionsPrivateSaveOpenTabsFunction
    : public UIThreadExtensionFunction {
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

class SessionsPrivateGetAllFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.getAll", SESSIONS_GETALL)
  SessionsPrivateGetAllFunction() = default;

 private:
  ~SessionsPrivateGetAllFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateGetAllFunction);
};

class SessionsPrivateOpenFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.open", SESSIONS_OPEN)
  SessionsPrivateOpenFunction() = default;

 private:
  ~SessionsPrivateOpenFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateOpenFunction);
};

class SessionsPrivateDeleteFunction : public UIThreadExtensionFunction {
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

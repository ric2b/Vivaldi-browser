//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_
#define EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/schema/vivaldi_sessions.h"

namespace extensions {

class SessionsPrivateSaveOpenTabsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.saveOpenTabs",
                             SESSIONS_SAVEOPENTABS)
  SessionsPrivateSaveOpenTabsFunction();

 protected:
  ~SessionsPrivateSaveOpenTabsFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateSaveOpenTabsFunction);
};

class SessionsPrivateGetAllFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.getAll", SESSIONS_GETALL)
  SessionsPrivateGetAllFunction();

 protected:
  ~SessionsPrivateGetAllFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateGetAllFunction);
};

class SessionsPrivateOpenFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.open", SESSIONS_OPEN)
  SessionsPrivateOpenFunction();

 protected:
  ~SessionsPrivateOpenFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateOpenFunction);
};

class SessionsPrivateDeleteFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.delete", SESSIONS_DELETE)
  SessionsPrivateDeleteFunction();

 protected:
  ~SessionsPrivateDeleteFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionsPrivateDeleteFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_

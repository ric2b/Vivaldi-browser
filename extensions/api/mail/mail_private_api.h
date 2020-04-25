// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_MAIL_MAIL_PRIVATE_API_H_
#define EXTENSIONS_API_MAIL_MAIL_PRIVATE_API_H_

#include "base/files/file_path.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/mail_private.h"

namespace extensions {

class MailPrivateGetPathsFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.getPaths", MAIL_GET_PATHS)
 public:
  MailPrivateGetPathsFunction() = default;

 protected:
  ~MailPrivateGetPathsFunction() override = default;
  void OnFinished(const std::vector<base::FilePath::StringType>& string_paths);

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateGetPathsFunction);
};

class MailPrivateSaveBufferFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.saveBuffer", MAIL_SAVE_FILE_BUFFER)
 public:
  MailPrivateSaveBufferFunction() = default;

 protected:
  ~MailPrivateSaveBufferFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateSaveBufferFunction);
};

class MailPrivateSaveFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.save", MAIL_SAVE)
 public:
  MailPrivateSaveFunction() = default;

 protected:
  ~MailPrivateSaveFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateSaveFunction);
};

class MailPrivateDeleteFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.delete", MAIL_DELETE)
 public:
  MailPrivateDeleteFunction() = default;

 protected:
  ~MailPrivateDeleteFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateDeleteFunction);
};

struct ReadFileResult {
  bool success;
  std::string raw;
};

class MailPrivateReadFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.read", MAIL_READ)
 public:
  MailPrivateReadFunction() = default;

 protected:
  ~MailPrivateReadFunction() override = default;
  void OnFinished(ReadFileResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateReadFunction);
};

class MailPrivateReadBufferFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.readBuffer", MAIL_READ_FILE_BUFFER)
 public:
  MailPrivateReadBufferFunction() = default;

 protected:
  ~MailPrivateReadBufferFunction() override = default;
  void OnFinished(ReadFileResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateReadBufferFunction);
};

class MailPrivateReadFileFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.readFile", MAIL_READ_FILE)
 public:
  MailPrivateReadFileFunction() = default;

 protected:
  ~MailPrivateReadFileFunction() override = default;
  void OnFinished(ReadFileResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateReadFileFunction);
};

struct GetDataDirectoryResult {
  bool success;
  base::FilePath::StringType path;
};

class MailPrivateGetDataDirectoryFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.getDataDirectory",
                             MAIL_GET_DATA_DIRECTORY)
 public:
  MailPrivateGetDataDirectoryFunction() = default;

 protected:
  ~MailPrivateGetDataDirectoryFunction() override = default;
  void OnFinished(GetDataDirectoryResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateGetDataDirectoryFunction);
};

class MailPrivateCreateDataDirectoryFunction
    : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.createDataDirectory",
                             MAIL_CREATE_DATA_DIRECTORY)
 public:
  MailPrivateCreateDataDirectoryFunction() = default;

 protected:
  ~MailPrivateCreateDataDirectoryFunction() override = default;
  void OnFinished(GetDataDirectoryResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateCreateDataDirectoryFunction);
};

class MailPrivateRenameFunction : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.rename", MAIL_RENAME)
 public:
  MailPrivateRenameFunction() = default;

 protected:
  ~MailPrivateRenameFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateRenameFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_MAIL_MAIL_PRIVATE_API_H_

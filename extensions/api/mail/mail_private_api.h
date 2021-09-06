// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_MAIL_MAIL_PRIVATE_API_H_
#define EXTENSIONS_API_MAIL_MAIL_PRIVATE_API_H_

#include "base/files/file_path.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/mail_private.h"

namespace extensions {

class MailPrivateGetFilePathsFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.getFilePaths",
                             MAIL_GET_FILE_PATHS)
 public:
  MailPrivateGetFilePathsFunction() = default;

 protected:
  ~MailPrivateGetFilePathsFunction() override = default;
  void OnFinished(const std::vector<base::FilePath::StringType>& string_paths);

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateGetFilePathsFunction);
};

class MailPrivateWriteBufferToMessageFileFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.writeBufferToMessageFile",
                             MAIL_WRITE_BUFFER_TO_MESSAGE_FILE)
 public:
  MailPrivateWriteBufferToMessageFileFunction() = default;

 protected:
  ~MailPrivateWriteBufferToMessageFileFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateWriteBufferToMessageFileFunction);
};

class MailPrivateWriteTextToMessageFileFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.writeTextToMessageFile",
                             MAIL_WRITE_TEXT_TO_MESSAGE_FILE)
 public:
  MailPrivateWriteTextToMessageFileFunction() = default;

 protected:
  ~MailPrivateWriteTextToMessageFileFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateWriteTextToMessageFileFunction);
};

class MailPrivateDeleteMessageFileFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.deleteMessageFile",
                             MAIL_DELETE_MESSAGE_FILE)
 public:
  MailPrivateDeleteMessageFileFunction() = default;

 protected:
  ~MailPrivateDeleteMessageFileFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateDeleteMessageFileFunction);
};

struct ReadFileResult {
  bool success;
  std::string raw;
};

class MailPrivateReadFileToBufferFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.readFileToBuffer",
                             MAIL_READ_FILE_TO_BUFFER)
 public:
  MailPrivateReadFileToBufferFunction() = default;

 protected:
  ~MailPrivateReadFileToBufferFunction() override = default;
  void OnFinished(ReadFileResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateReadFileToBufferFunction);
};

class MailPrivateReadMessageFileToBufferFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.readMessageFileToBuffer",
                             MAIL_READ_MESSAGE_FILE_TO_BUFFER)
 public:
  MailPrivateReadMessageFileToBufferFunction() = default;

 protected:
  ~MailPrivateReadMessageFileToBufferFunction() override = default;
  void OnFinished(ReadFileResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateReadMessageFileToBufferFunction);
};

class MailPrivateReadFileToTextFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.readFileToText",
                             MAIL_READ_FILE_TO_TEXT)
 public:
  MailPrivateReadFileToTextFunction() = default;

 protected:
  ~MailPrivateReadFileToTextFunction() override = default;
  void OnFinished(ReadFileResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateReadFileToTextFunction);
};

struct GetDirectoryResult {
  bool success;
  base::FilePath::StringType path;
};

class MailPrivateGetFileDirectoryFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.getFileDirectory",
                             MAIL_GET_FILE_DIRECTORY)
 public:
  MailPrivateGetFileDirectoryFunction() = default;

 protected:
  ~MailPrivateGetFileDirectoryFunction() override = default;
  void OnFinished(GetDirectoryResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateGetFileDirectoryFunction);
};

class MailPrivateCreateFileDirectoryFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.createFileDirectory",
                             MAIL_CREATE_FILE_DIRECTORY)
 public:
  MailPrivateCreateFileDirectoryFunction() = default;

 protected:
  ~MailPrivateCreateFileDirectoryFunction() override = default;
  void OnFinished(GetDirectoryResult result);
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailPrivateCreateFileDirectoryFunction);
};
}  // namespace extensions

#endif  // EXTENSIONS_API_MAIL_MAIL_PRIVATE_API_H_

// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/mail/mail_private_api.h"

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"

using base::Value;

namespace {
const base::FilePath::CharType kMailDirectory[] = FILE_PATH_LITERAL("Mail");

bool deleteFile(base::FilePath file_path,
                base::FilePath::StringType file_name) {
  if (file_name.length() > 0) {
    file_path = file_path.Append(file_name);
  }

  if (!file_path.IsAbsolute()) {
    return false;
  }

  if (!base::PathExists(file_path)) {
    return false;
  }

  return base::DeleteFile(file_path);
}

base::FilePath::StringType FilePathAsString(const base::FilePath& path) {
#if defined(OS_WIN)
  return path.value();
#else
  return path.value().c_str();
#endif
}

base::FilePath::StringType StringToStringType(const std::string& str) {
#if defined(OS_WIN)
  return base::UTF8ToUTF16(str);
#else
  return str;
#endif
}

std::string StringTypeToString(const base::FilePath::StringType& str) {
#if defined(OS_WIN)
  return base::UTF16ToUTF8(str);
#else
  return str;
#endif
}

std::vector<base::FilePath::StringType> FindMailFiles(
    base::FilePath file_path) {
  base::FileEnumerator file_enumerator(
      file_path, true, base::FileEnumerator::FILES, FILE_PATH_LITERAL("*"),
      base::FileEnumerator::FolderSearchPolicy::ALL);

  std::vector<base::FilePath::StringType> string_paths;
  for (base::FilePath locale_file_path = file_enumerator.Next();
       !locale_file_path.empty(); locale_file_path = file_enumerator.Next()) {
    string_paths.push_back(FilePathAsString(locale_file_path));
  }

  return string_paths;
}

}  // namespace

namespace extensions {

namespace mail = vivaldi::mail_private;

ExtensionFunction::ResponseAction MailPrivateGetFilePathsFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::GetFilePaths::Params> params(
      vivaldi::mail_private::GetFilePaths::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(params->path);

  if (!file_path.IsAbsolute()) {
    return RespondNow(Error(base::StringPrintf(
        "Path must be absolute %s", file_path.AsUTF8Unsafe().c_str())));
  }

  if (!base::DirectoryExists(file_path)) {
    return RespondNow(Error(base::StringPrintf(
        "Directory does not exist %s", file_path.AsUTF8Unsafe().c_str())));
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&FindMailFiles, file_path),
      base::BindOnce(&MailPrivateGetFilePathsFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateGetFilePathsFunction::OnFinished(
    const std::vector<base::FilePath::StringType>& results) {
  std::vector<std::string> string_paths;
  for (const auto& result : results) {
    string_paths.push_back(StringTypeToString(result));
  }
  Respond(ArgumentList(
      extensions::vivaldi::mail_private::GetFilePaths::Results::Create(
          string_paths)));
}

base::FilePath GetSavePath(base::FilePath file_path,
                           std::vector<base::FilePath::StringType> string_paths,
                           base::FilePath::StringType file_name) {
  base::FilePath data_dir;
  size_t count = string_paths.size();

  file_path = file_path.Append(kMailDirectory);
  for (size_t i = 0; i < count; i++) {
    file_path = file_path.Append(string_paths[i]);
    if (!base::DirectoryExists(file_path))
      base::CreateDirectory(file_path);
  }

  if (file_name.length() > 0) {
    file_path = file_path.Append(file_name);
  }

  return file_path;
}

bool Save(base::FilePath file_path,
          std::vector<base::FilePath::StringType> string_paths,
          base::FilePath::StringType file_name,
          std::string data) {
  file_path = GetSavePath(file_path, string_paths, file_name);
  if (!file_path.IsAbsolute()) {
    return false;
  }

  int size = data.size();
  int wrote = base::WriteFile(file_path, data.data(), size);
  return size == wrote;
}

bool SaveBuffer(base::FilePath file_path,
                std::vector<base::FilePath::StringType> string_paths,
                base::FilePath::StringType file_name,
                const std::vector<uint8_t>& data) {
  file_path = GetSavePath(file_path, string_paths, file_name);
  if (!file_path.IsAbsolute()) {
    return false;
  }
  return base::WriteFile(file_path, reinterpret_cast<const char*>(data.data()),
                         data.size()) != -1;
}

GetDirectoryResult CreateDirectory(base::FilePath file_path,
                                   std::string directory) {
  GetDirectoryResult result;

  file_path = file_path.Append(kMailDirectory);
  file_path = file_path.AppendASCII(directory);

  if (!file_path.IsAbsolute()) {
    result.success = false;
  }

  if (!base::DirectoryExists(file_path)) {
    result.success = base::CreateDirectory(file_path);
    result.path = FilePathAsString(file_path);
  }

  return result;
}

ExtensionFunction::ResponseAction
MailPrivateWriteTextToMessageFileFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::WriteTextToMessageFile::Params> params(
      vivaldi::mail_private::WriteTextToMessageFile::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<base::FilePath::StringType> string_paths;
  for (const auto& path : params->paths) {
    string_paths.push_back(StringToStringType(path));
  }

  base::FilePath::StringType file_name = StringToStringType(params->file_name);
  std::string data = params->raw;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Save, file_path, string_paths, file_name, data),
      base::BindOnce(&MailPrivateWriteTextToMessageFileFunction::OnFinished,
                     this));

  return RespondLater();
}
void MailPrivateWriteTextToMessageFileFunction::OnFinished(bool result) {
  if (result == true) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StringPrintf("Error saving file")));
  }
}

ExtensionFunction::ResponseAction
MailPrivateWriteBufferToMessageFileFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::WriteBufferToMessageFile::Params>
      params(vivaldi::mail_private::WriteBufferToMessageFile::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<base::FilePath::StringType> string_paths;
  for (const auto& path : params->paths) {
    string_paths.push_back(StringToStringType(path));
  }

  base::FilePath::StringType file_name = StringToStringType(params->file_name);
  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SaveBuffer, file_path, string_paths, file_name,
                     params->raw),
      base::BindOnce(&MailPrivateWriteBufferToMessageFileFunction::OnFinished,
                     this));

  return RespondLater();
}
void MailPrivateWriteBufferToMessageFileFunction::OnFinished(bool result) {
  if (result == true) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StringPrintf("Error saving file")));
  }
}

bool Delete(base::FilePath file_path, base::FilePath::StringType file_name) {
  return deleteFile(file_path, file_name);
}

ExtensionFunction::ResponseAction MailPrivateDeleteMessageFileFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::DeleteMessageFile::Params> params(
      vivaldi::mail_private::DeleteMessageFile::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<std::string>& string_paths = params->paths;
  base::FilePath::StringType file_name = StringToStringType(params->file_name);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.Append(kMailDirectory);

  size_t count = string_paths.size();

  for (size_t i = 0; i < count; i++) {
    file_path = file_path.AppendASCII(string_paths[i]);
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Delete, file_path, file_name),
      base::BindOnce(&MailPrivateDeleteMessageFileFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateDeleteMessageFileFunction::OnFinished(bool result) {
  if (result == true) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StringPrintf("Error deleting file")));
  }
}

ReadFileResult Read(base::FilePath file_path) {
  std::string raw = "";
  ReadFileResult result;

  if (!base::PathExists(file_path)) {
    result.success = false;
    return result;
  }

  if (base::ReadFileToString(file_path, &raw)) {
    result.raw = raw;
    result.success = true;

  } else {
    result.success = false;
  }
  return result;
}

ExtensionFunction::ResponseAction MailPrivateReadFileToBufferFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::ReadFileToBuffer::Params> params(
      vivaldi::mail_private::ReadFileToBuffer::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(params->file_name);
  if (!file_path.IsAbsolute()) {
    return RespondNow(Error(base::StringPrintf(
        "Path must be absolute %s", file_path.AsUTF8Unsafe().c_str())));
  }

  if (!base::PathExists(file_path)) {
    return RespondNow(Error(base::StringPrintf(
        "File path does not exist %s", file_path.AsUTF8Unsafe().c_str())));
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Read, file_path),
      base::BindOnce(&MailPrivateReadFileToBufferFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateReadFileToBufferFunction::OnFinished(ReadFileResult result) {
  if (result.success == true) {
    Respond(
        OneArgument(base::Value(base::as_bytes(base::make_span(result.raw)))));
  } else {
    Respond(Error(base::StringPrintf("Error reading file")));
  }
}
ExtensionFunction::ResponseAction
MailPrivateReadMessageFileToBufferFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::ReadMessageFileToBuffer::Params>
      params(vivaldi::mail_private::ReadMessageFileToBuffer::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<std::string>& string_paths = params->paths;
  std::string file_name = params->file_name;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.Append(kMailDirectory);

  size_t count = string_paths.size();

  for (size_t i = 0; i < count; i++) {
    file_path = file_path.AppendASCII(string_paths[i]);
  }

  if (file_name.length() > 0) {
    file_path = file_path.AppendASCII(file_name);
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Read, file_path),
      base::BindOnce(&MailPrivateReadMessageFileToBufferFunction::OnFinished,
                     this));

  return RespondLater();
}

void MailPrivateReadMessageFileToBufferFunction::OnFinished(
    ReadFileResult result) {
  if (result.success == true) {
    Respond(
        OneArgument(base::Value(base::as_bytes(base::make_span(result.raw)))));
  } else {
    Respond(Error(base::StringPrintf("Error reading file")));
  }
}

ExtensionFunction::ResponseAction MailPrivateReadFileToTextFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::ReadFileToText::Params> params(
      vivaldi::mail_private::ReadFileToText::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string path = params->path;
  base::FilePath base_path;
  base::FilePath file_path = base_path.AppendASCII(path);

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Read, file_path),
      base::BindOnce(&MailPrivateReadFileToTextFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateReadFileToTextFunction::OnFinished(ReadFileResult result) {
  if (result.success == true) {
    Respond(ArgumentList(
        extensions::vivaldi::mail_private::ReadFileToText::Results::Create(
            result.raw)));
  } else {
    Respond(Error(base::StringPrintf("Error reading file")));
  }
}

GetDirectoryResult GetDirectory(base::FilePath file_path) {
  std::string path = "";
  GetDirectoryResult result;

  if (!base::PathExists(file_path)) {
    result.success = false;
    return result;
  }

  result.path = FilePathAsString(file_path);
  result.success = true;

  return result;
}

ExtensionFunction::ResponseAction MailPrivateGetFileDirectoryFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::GetFileDirectory::Params> params(
      vivaldi::mail_private::GetFileDirectory::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath::StringType hashed_account_id =
      StringToStringType(params->hashed_account_id);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.Append(kMailDirectory);
  file_path = file_path.Append(hashed_account_id);

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetDirectory, file_path),
      base::BindOnce(&MailPrivateGetFileDirectoryFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateGetFileDirectoryFunction::OnFinished(
    GetDirectoryResult result) {
  if (result.success == true) {
    Respond(ArgumentList(
        extensions::vivaldi::mail_private::GetFileDirectory::Results::Create(
            StringTypeToString(result.path))));
  } else {
    Respond(Error("Directory not found"));
  }
}

ExtensionFunction::ResponseAction
MailPrivateCreateFileDirectoryFunction::Run() {
  std::unique_ptr<
      extensions::vivaldi::mail_private::CreateFileDirectory::Params>
      params(extensions::vivaldi::mail_private::CreateFileDirectory::Params::
                 Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string hashed_account_id = params->hashed_account_id;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CreateDirectory, file_path, hashed_account_id),
      base::BindOnce(&MailPrivateCreateFileDirectoryFunction::OnFinished,
                     this));

  return RespondLater();
}

void MailPrivateCreateFileDirectoryFunction::OnFinished(
    GetDirectoryResult result) {
  if (result.success == true) {
    Respond(ArgumentList(
        extensions::vivaldi::mail_private::CreateFileDirectory::Results::Create(
            StringTypeToString(result.path))));
  } else {
    Respond(Error("Directory not created"));
  }
}
}  //  namespace extensions

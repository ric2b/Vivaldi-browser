// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/mail/mail_private_api.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
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

  return base::DeleteFile(file_path, false);
}

bool Rename(base::FilePath file_path,
            base::FilePath::StringType old_file_name,
            base::FilePath::StringType new_file_name) {
  base::FilePath new_file_path = file_path;

  if (old_file_name.length() > 0) {
    file_path = file_path.Append(old_file_name);
  }

  if (new_file_name.length() > 0) {
    new_file_path = new_file_path.Append(new_file_name);
  }

  if (!file_path.IsAbsolute()) {
    return false;
  }

  if (!new_file_path.IsAbsolute()) {
    return false;
  }

  if (!base::PathExists(file_path)) {
    return false;
  }

  return base::Move(file_path, new_file_path);
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
      file_path, true, base::FileEnumerator::FILES, FILE_PATH_LITERAL("*.*"));

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

ExtensionFunction::ResponseAction MailPrivateGetPathsFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());

  std::unique_ptr<vivaldi::mail_private::GetPaths::Params> params(
      vivaldi::mail_private::GetPaths::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath file_path = profile->GetPath();
  file_path = file_path.Append(kMailDirectory);

  std::vector<base::FilePath::StringType> string_paths;
  for (const auto& path : params->paths) {
    string_paths.push_back(StringToStringType(path));
  }

  size_t count = string_paths.size();

  for (size_t i = 0; i < count; i++) {
    file_path = file_path.Append(string_paths[i]);
  }

  if (!file_path.IsAbsolute()) {
    return RespondNow(Error(base::StringPrintf(
        "Path must be absolute %s", file_path.AsUTF8Unsafe().c_str())));
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&FindMailFiles, file_path),
      base::Bind(&MailPrivateGetPathsFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateGetPathsFunction::OnFinished(
    const std::vector<base::FilePath::StringType>& results) {
  std::vector<std::string> string_paths;
  for (const auto& result : results) {
    string_paths.push_back(StringTypeToString(result));
  }
  Respond(
      ArgumentList(extensions::vivaldi::mail_private::GetPaths::Results::Create(
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

GetDataDirectoryResult CreateDirectory(base::FilePath file_path,
                                       std::string directory) {
  GetDataDirectoryResult result;

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

ExtensionFunction::ResponseAction MailPrivateSaveFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::Save::Params> params(
      vivaldi::mail_private::Save::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<base::FilePath::StringType> string_paths;
  for (const auto& path : params->paths) {
    string_paths.push_back(StringToStringType(path));
  }

  base::FilePath::StringType file_name = StringToStringType(params->file_name);
  std::string data = params->raw;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&Save, file_path, string_paths, file_name, data),
      base::Bind(&MailPrivateSaveFunction::OnFinished, this));

  return RespondLater();
}
void MailPrivateSaveFunction::OnFinished(bool result) {
  if (result == true) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StringPrintf("Error saving file")));
  }
}

ExtensionFunction::ResponseAction MailPrivateSaveBufferFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::SaveBuffer::Params> params(
      vivaldi::mail_private::SaveBuffer::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<base::FilePath::StringType> string_paths;
  for (const auto& path : params->paths) {
    string_paths.push_back(StringToStringType(path));
  }

  base::FilePath::StringType file_name = StringToStringType(params->file_name);
  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&SaveBuffer, file_path, string_paths, file_name, params->raw),
      base::Bind(&MailPrivateSaveBufferFunction::OnFinished, this));

  return RespondLater();
}
void MailPrivateSaveBufferFunction::OnFinished(bool result) {
  if (result == true) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StringPrintf("Error saving file")));
  }
}

bool Delete(base::FilePath file_path, base::FilePath::StringType file_name) {
  return deleteFile(file_path, file_name);
}

ExtensionFunction::ResponseAction MailPrivateDeleteFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::Delete::Params> params(
      vivaldi::mail_private::Delete::Params::Create(*args_));
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

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&Delete, file_path, file_name),
      base::Bind(&MailPrivateDeleteFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateDeleteFunction::OnFinished(bool result) {
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

ExtensionFunction::ResponseAction MailPrivateReadFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::Read::Params> params(
      vivaldi::mail_private::Read::Params::Create(*args_));
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

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&Read, file_path),
      base::Bind(&MailPrivateReadFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateReadFunction::OnFinished(ReadFileResult result) {
  if (result.success == true) {
    Respond(ArgumentList(
        extensions::vivaldi::mail_private::Read::Results::Create(result.raw)));
  } else {
    Respond(Error(base::StringPrintf("Error reading file")));
  }
}
ExtensionFunction::ResponseAction MailPrivateReadBufferFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::ReadBuffer::Params> params(
      vivaldi::mail_private::ReadBuffer::Params::Create(*args_));
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

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&Read, file_path),
      base::Bind(&MailPrivateReadBufferFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateReadBufferFunction::OnFinished(ReadFileResult result) {
  if (result.success == true) {
    Respond(OneArgument(Value::CreateWithCopiedBuffer((&result.raw)->c_str(),
                                                      (&result.raw)->size())));
  } else {
    Respond(Error(base::StringPrintf("Error reading file")));
  }
}

ExtensionFunction::ResponseAction MailPrivateReadFileFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::ReadFile::Params> params(
      vivaldi::mail_private::ReadFile::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string path = params->path;
  base::FilePath base_path;
  base::FilePath file_path = base_path.AppendASCII(path);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&Read, file_path),
      base::Bind(&MailPrivateReadFileFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateReadFileFunction::OnFinished(ReadFileResult result) {
  if (result.success == true) {
    Respond(ArgumentList(
        extensions::vivaldi::mail_private::ReadFile::Results::Create(
            result.raw)));
  } else {
    Respond(Error(base::StringPrintf("Error reading file")));
  }
}

GetDataDirectoryResult GetDataDirectory(base::FilePath file_path) {
  std::string path = "";
  GetDataDirectoryResult result;

  if (!base::PathExists(file_path)) {
    result.success = false;
    return result;
  }

  result.path = FilePathAsString(file_path);
  result.success = true;

  return result;
}

ExtensionFunction::ResponseAction MailPrivateGetDataDirectoryFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::GetDataDirectory::Params> params(
      vivaldi::mail_private::GetDataDirectory::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath::StringType hashed_account_id =
      StringToStringType(params->hashed_account_id);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.Append(kMailDirectory);
  file_path = file_path.Append(hashed_account_id);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&GetDataDirectory, file_path),
      base::Bind(&MailPrivateGetDataDirectoryFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateGetDataDirectoryFunction::OnFinished(
    GetDataDirectoryResult result) {
  if (result.success == true) {
    Respond(ArgumentList(
        extensions::vivaldi::mail_private::GetDataDirectory::Results::Create(
            StringTypeToString(result.path))));
  } else {
    Respond(Error("Directory not found"));
  }
}

ExtensionFunction::ResponseAction
MailPrivateCreateDataDirectoryFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::CreateDataDirectory::Params> params(
      vivaldi::mail_private::CreateDataDirectory::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string hashed_account_id = params->hashed_account_id;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&CreateDirectory, file_path, hashed_account_id),
      base::Bind(&MailPrivateCreateDataDirectoryFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateCreateDataDirectoryFunction::OnFinished(
    GetDataDirectoryResult result) {
  if (result.success == true) {
    Respond(ArgumentList(
        extensions::vivaldi::mail_private::CreateDataDirectory::Results::Create(
            StringTypeToString(result.path))));
  } else {
    Respond(Error("Directory not created"));
  }
}

ExtensionFunction::ResponseAction MailPrivateRenameFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::Rename::Params> params(
      vivaldi::mail_private::Rename::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<base::FilePath::StringType> string_paths;
  for (const auto& path : params->paths) {
    string_paths.push_back(StringToStringType(path));
  }

  base::FilePath::StringType file_name = StringToStringType(params->file_name);
  base::FilePath::StringType new_file_name =
      StringToStringType(params->new_file_name);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.Append(kMailDirectory);

  size_t count = string_paths.size();

  for (size_t i = 0; i < count; i++) {
    file_path = file_path.Append(string_paths[i]);
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&Rename, file_path, file_name, new_file_name),
      base::Bind(&MailPrivateRenameFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateRenameFunction::OnFinished(bool result) {
  if (result == true) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StringPrintf("Error renaming file")));
  }
}

}  //  namespace extensions

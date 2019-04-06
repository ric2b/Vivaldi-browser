// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/mail/mail_private_api.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/profiles/profile.h"

namespace {
const char kMailDirectory[] = "Mail";
const char kMailFileEnding[] = ".vmail";
const char kFlagsFileEnding[] = ".vflags";

bool deleteFile(base::FilePath file_path,
                std::string file_name,
                std::string ending) {
  if (file_name.length() > 0) {
    file_path = file_path.AppendASCII(file_name + ending);
  }

  if (!file_path.IsAbsolute()) {
    return false;
  }

  if (!base::PathExists(file_path)) {
    return false;
  }

  return base::DeleteFile(file_path, false);
}

std::string FilePathAsString(const base::FilePath& path) {
#if defined(OS_WIN)
  return base::UTF16ToUTF8(path.value());
#else
  return path.value().c_str();
#endif
}

std::vector<std::string> FindMailFiles(base::FilePath file_path) {
  base::FileEnumerator file_enumerator(
      file_path, true, base::FileEnumerator::FILES, FILE_PATH_LITERAL("*.*"));

  std::vector<std::string> string_paths;

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
  file_path = file_path.AppendASCII(kMailDirectory);

  std::vector<std::string>& string_paths = params->paths;
  size_t count = string_paths.size();

  for (size_t i = 0; i < count; i++) {
    file_path = file_path.AppendASCII(string_paths[i]);
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
    const std::vector<std::string>& string_paths) {
  Respond(
      ArgumentList(extensions::vivaldi::mail_private::GetPaths::Results::Create(
          string_paths)));
}

bool Save(base::FilePath file_path,
          std::vector<std::string> string_paths,
          std::string file_name,
          std::string data) {
  size_t count = string_paths.size();

  file_path = file_path.AppendASCII(kMailDirectory);

  if (!file_path.IsAbsolute()) {
    return false;
  }

  for (size_t i = 0; i < count; i++) {
    file_path = file_path.AppendASCII(string_paths[i]);
    if (!base::DirectoryExists(file_path))
      base::CreateDirectory(file_path);
  }

  if (file_name.length() > 0) {
    file_path = file_path.AppendASCII(file_name);
  }

  std::vector<base::FilePath> paths;
  paths.push_back(file_path);

  int size = data.size();
  int wrote = base::WriteFile(file_path, data.data(), size);
  return size == wrote;
}

GetDataDirectoryResult CreateDirectory(base::FilePath file_path,
                                       std::string directory) {
  GetDataDirectoryResult result;

  file_path = file_path.AppendASCII(kMailDirectory);
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

  std::vector<std::string>& string_paths = params->paths;
  std::string file_name = params->file_name;
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

bool Delete(base::FilePath file_path, std::string file_name) {
  return (deleteFile(file_path, file_name, kMailFileEnding) &&
          deleteFile(file_path, file_name, kFlagsFileEnding));
}

ExtensionFunction::ResponseAction MailPrivateDeleteFunction::Run() {
  std::unique_ptr<vivaldi::mail_private::Delete::Params> params(
      vivaldi::mail_private::Delete::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<std::string>& string_paths = params->paths;
  std::string file_name = params->file_name;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.AppendASCII(kMailDirectory);

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

  file_path = file_path.AppendASCII(kMailDirectory);

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

  std::string hashed_account_id = params->hashed_account_id;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.AppendASCII(kMailDirectory);
  file_path = file_path.AppendASCII(hashed_account_id);

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
            result.path)));
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
            result.path)));
  } else {
    Respond(Error("Directory not created"));
  }
}

}  //  namespace extensions

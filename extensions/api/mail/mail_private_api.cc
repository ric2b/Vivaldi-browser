// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/mail/mail_private_api.h"

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/db/mail_client/mail_client_service_factory.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/schema/mail_private.h"

using base::Value;
using mail_client::MailClientService;
using mail_client::MailClientServiceFactory;

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

bool renameFile(base::FilePath file_path,
                base::FilePath::StringType file_name,
                base::FilePath::StringType new_file_name) {
  if (file_name.length() == 0) {
    return false;
  }

  if (new_file_name.length() == 0) {
    return false;
  }

  if (!file_path.IsAbsolute()) {
    return false;
  }

  file_path = file_path.Append(file_name);
  base::FilePath new_file_path = file_path.Append(new_file_name);

  if (!base::PathExists(file_path)) {
    return false;
  }

  if (base::PathExists(new_file_path)) {
    return false;
  }

  return base::Move(file_path, new_file_path);
}

base::FilePath::StringType FilePathAsString(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  return path.value();
#else
  return path.value().c_str();
#endif
}

base::FilePath::StringType StringToStringType(const std::string& str) {
#if BUILDFLAG(IS_WIN)
  return base::UTF8ToWide(str);
#else
  return str;
#endif
}

std::string StringTypeToString(const base::FilePath::StringType& str) {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(str);
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

namespace mail_private = vivaldi::mail_private;

namespace OnUpgradeProgress = vivaldi::mail_private::OnUpgradeProgress;
namespace OnDeleteMessagesProgress =
    vivaldi::mail_private::OnDeleteMessagesProgress;

Profile* MailPrivateAsyncFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

MailClientService* MailPrivateAsyncFunction::GetMailClientService() {
  return MailClientServiceFactory::GetForProfile(GetProfile());
}

MailEventRouter::MailEventRouter(Profile* profile,
                                 MailClientService* mail_client_service)
    : profile_(profile) {
  DCHECK(profile);
  mail_service_observation_.Observe(mail_client_service);
}

MailEventRouter::~MailEventRouter() {}

void MailEventRouter::OnMigrationProgress(MailClientService* service,
                                          int progress,
                                          int total,
                                          std::string msg) {
  base::Value::List args = OnUpgradeProgress::Create(progress, total, msg);
  DispatchEvent(profile_, OnUpgradeProgress::kEventName, std::move(args));
}

void MailEventRouter::OnDeleteMessagesProgress(MailClientService* service,
                                               int total) {
  base::Value::List args = OnDeleteMessagesProgress::Create(total);
  DispatchEvent(profile_, OnDeleteMessagesProgress::kEventName,
                std::move(args));
}

// Helper to actually dispatch an event to extension listeners.
void MailEventRouter::DispatchEvent(Profile* profile,
                                    const std::string& event_name,
                                    base::Value::List event_args) {
  if (profile && EventRouter::Get(profile)) {
    EventRouter* event_router = EventRouter::Get(profile);
    if (event_router) {
      event_router->BroadcastEvent(base::WrapUnique(
          new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                                event_name, std::move(event_args))));
    }
  }
}

MailAPI::MailAPI(content::BrowserContext* context) : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, OnUpgradeProgress::kEventName);
  event_router->RegisterObserver(this, OnDeleteMessagesProgress::kEventName);
}

MailAPI::~MailAPI() {}

void MailAPI::Shutdown() {
  mail_client_event_router_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<MailAPI>>::DestructorAtExit g_factory_mail =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MailAPI>* MailAPI::GetFactoryInstance() {
  return g_factory_mail.Pointer();
}

void MailAPI::OnListenerAdded(const EventListenerInfo& details) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  mail_client_event_router_ = std::make_unique<MailEventRouter>(
      profile, MailClientServiceFactory::GetForProfile(profile));

  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

ExtensionFunction::ResponseAction MailPrivateGetFilePathsFunction::Run() {
  std::optional<mail_private::GetFilePaths::Params> params(
      mail_private::GetFilePaths::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(params->path);

  if (!file_path.IsAbsolute()) {
    return RespondNow(Error(base::StringPrintf(
        "Path must be absolute %s", file_path.AsUTF8Unsafe().c_str())));
  }
  file_path_ = file_path;
  base::OnceCallback<void(bool)> check_directory_callback =
      base::BindOnce(&MailPrivateGetFilePathsFunction::GetFilePaths, this);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&base::DirectoryExists, file_path),
      std::move(check_directory_callback));

  return RespondLater();
}

void MailPrivateGetFilePathsFunction::GetFilePaths(bool directory_exists) {
  if (!directory_exists) {
    Respond(Error(base::StringPrintf("Directory does not exist %s",
                                     file_path_.AsUTF8Unsafe().c_str())));
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&FindMailFiles, file_path_),
      base::BindOnce(&MailPrivateGetFilePathsFunction::OnFinished, this));
}

ExtensionFunction::ResponseAction MailPrivateGetFullPathFunction::Run() {
  std::optional<mail_private::GetFullPath::Params> params(
      mail_private::GetFullPath::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& filesystem_name = params->filesystem;
  const std::string& filesystem_path = params->path;

  base::FilePath file_path;
  std::string error;
  if (!app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path, source_process_id(), &file_path,
          &error)) {
    return RespondNow(Error(std::move(error)));
  }

  return RespondNow(WithArguments(base::Value(file_path.AsUTF8Unsafe())));
}

void MailPrivateGetFilePathsFunction::OnFinished(
    const std::vector<base::FilePath::StringType>& results) {
  std::vector<std::string> string_paths;
  for (const auto& result : results) {
    string_paths.push_back(StringTypeToString(result));
  }
  Respond(
      ArgumentList(mail_private::GetFilePaths::Results::Create(string_paths)));
}

ExtensionFunction::ResponseAction MailPrivateGetMailFilePathsFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.Append(kMailDirectory);

  if (!file_path.IsAbsolute()) {
    return RespondNow(Error(base::StringPrintf(
        "Path must be absolute %s", file_path.AsUTF8Unsafe().c_str())));
  }

  file_path_ = file_path;
  base::OnceCallback<void(bool)> check_directory_callback = base::BindOnce(
      &MailPrivateGetMailFilePathsFunction::GetMailFilePaths, this);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&base::DirectoryExists, file_path),
      std::move(check_directory_callback));

  return RespondLater();
}

void MailPrivateGetMailFilePathsFunction::GetMailFilePaths(
    bool directory_exists) {
  if (!directory_exists) {
    Respond(Error(base::StringPrintf("Directory does not exist %s",
                                     file_path_.AsUTF8Unsafe().c_str())));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&FindMailFiles, file_path_),
      base::BindOnce(&MailPrivateGetMailFilePathsFunction::OnFinished, this));
}

void MailPrivateGetMailFilePathsFunction::OnFinished(
    const std::vector<base::FilePath::StringType>& results) {
  std::vector<std::string> string_paths;
  for (const auto& result : results) {
    string_paths.push_back(StringTypeToString(result));
  }
  Respond(
      ArgumentList(mail_private::GetFilePaths::Results::Create(string_paths)));
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
          std::string data,
          bool append) {
  file_path = GetSavePath(file_path, string_paths, file_name);
  if (!file_path.IsAbsolute()) {
    return false;
  }

  // AppendToFile returns true if all data was written, whereas WriteToFile
  // returns number of bytes and -1 on failure.
  if (append) {
    return base::AppendToFile(file_path, data.data());
  } else {
    return base::WriteFile(file_path, data);
  }
}

bool SaveBuffer(base::FilePath file_path,
                std::vector<base::FilePath::StringType> string_paths,
                base::FilePath::StringType file_name,
                const std::vector<uint8_t>& data,
                bool append) {
  file_path = GetSavePath(file_path, string_paths, file_name);
  if (!file_path.IsAbsolute()) {
    return false;
  }
  if (append) {
    return base::AppendToFile(file_path, data);
  } else {
    return base::WriteFile(file_path, data);
  }
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
  std::optional<mail_private::WriteTextToMessageFile::Params> params(
      mail_private::WriteTextToMessageFile::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<base::FilePath::StringType> string_paths;
  for (const auto& path : params->paths) {
    string_paths.push_back(StringToStringType(path));
  }

  base::FilePath::StringType file_name = StringToStringType(params->file_name);
  std::string data = params->raw;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  bool append = params->append ? *(params->append) : false;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Save, file_path, string_paths, file_name, data, append),
      base::BindOnce(&MailPrivateWriteTextToMessageFileFunction::OnFinished,
                     this));

  return RespondLater();
}

void MailPrivateWriteTextToMessageFileFunction::OnFinished(bool result) {
  if (result == true) {
    Respond(NoArguments());
  } else {
    Respond(Error("Error saving file"));
  }
}

ExtensionFunction::ResponseAction
MailPrivateWriteBufferToMessageFileFunction::Run() {
  std::optional<mail_private::WriteBufferToMessageFile::Params> params(
      mail_private::WriteBufferToMessageFile::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<base::FilePath::StringType> string_paths;
  for (const auto& path : params->paths) {
    string_paths.push_back(StringToStringType(path));
  }

  base::FilePath::StringType file_name = StringToStringType(params->file_name);
  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  bool append = params->append ? *(params->append) : false;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SaveBuffer, file_path, string_paths, file_name,
                     params->raw, append),
      base::BindOnce(&MailPrivateWriteBufferToMessageFileFunction::OnFinished,
                     this));

  return RespondLater();
}
void MailPrivateWriteBufferToMessageFileFunction::OnFinished(bool result) {
  if (result == true) {
    Respond(NoArguments());
  } else {
    Respond(Error("Error saving file"));
  }
}

bool Delete(base::FilePath file_path, base::FilePath::StringType file_name) {
  return deleteFile(file_path, file_name);
}

ExtensionFunction::ResponseAction MailPrivateDeleteMessageFileFunction::Run() {
  std::optional<mail_private::DeleteMessageFile::Params> params(
      mail_private::DeleteMessageFile::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<std::string>& string_paths = params->paths;
  base::FilePath::StringType file_name = StringToStringType(params->file_name);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.Append(kMailDirectory);

  size_t count = string_paths.size();

  for (size_t i = 0; i < count; i++) {
    file_path = file_path.AppendASCII(string_paths[i]);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
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

ExtensionFunction::ResponseAction MailPrivateRenameMessageFileFunction::Run() {
  std::optional<mail_private::RenameMessageFile::Params> params(
      mail_private::RenameMessageFile::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<std::string>& string_paths = params->paths;
  base::FilePath::StringType file_name = StringToStringType(params->file_name);
  base::FilePath::StringType new_file_name =
      StringToStringType(params->new_file_name);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.Append(kMailDirectory);

  size_t count = string_paths.size();

  for (size_t i = 0; i < count; i++) {
    file_path = file_path.AppendASCII(string_paths[i]);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&renameFile, file_path, file_name, new_file_name),
      base::BindOnce(&MailPrivateRenameMessageFileFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateRenameMessageFileFunction::OnFinished(bool result) {
  Respond(
      ArgumentList(mail_private::RenameMessageFile::Results::Create(result)));
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
  std::optional<mail_private::ReadFileToBuffer::Params> params(
      mail_private::ReadFileToBuffer::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(params->file_name);

  if (!file_path.IsAbsolute()) {
    return RespondNow(Error(base::StringPrintf(
        "Path must be absolute %s", file_path.AsUTF8Unsafe().c_str())));
  }

  if (!base::PathExists(file_path)) {
    return RespondNow(Error(base::StringPrintf(
        "File path does not exist %s", file_path.AsUTF8Unsafe().c_str())));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Read, file_path),
      base::BindOnce(&MailPrivateReadFileToBufferFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateReadFileToBufferFunction::OnFinished(ReadFileResult result) {
  if (result.success == true) {
    Respond(WithArguments(
        base::Value(base::as_bytes(base::make_span(result.raw)))));
  } else {
    Respond(Error(base::StringPrintf("Error reading file")));
  }
}
ExtensionFunction::ResponseAction MailPrivateMessageFileExistsFunction::Run() {
  std::optional<mail_private::MessageFileExists::Params> params(
      mail_private::MessageFileExists::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

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
  bool exists = base::PathExists(file_path);
  return RespondNow(
      ArgumentList(mail_private::MessageFileExists::Results::Create(exists)));
}

ExtensionFunction::ResponseAction
MailPrivateReadMessageFileToBufferFunction::Run() {
  std::optional<mail_private::ReadMessageFileToBuffer::Params> params(
      mail_private::ReadMessageFileToBuffer::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

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

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Read, file_path),
      base::BindOnce(&MailPrivateReadMessageFileToBufferFunction::OnFinished,
                     this));

  return RespondLater();
}

void MailPrivateReadMessageFileToBufferFunction::OnFinished(
    ReadFileResult result) {
  if (result.success == true) {
    Respond(WithArguments(
        base::Value(base::as_bytes(base::make_span(result.raw)))));
  } else {
    Respond(Error(base::StringPrintf("Error reading file")));
  }
}

ExtensionFunction::ResponseAction MailPrivateReadFileToTextFunction::Run() {
  std::optional<mail_private::ReadFileToText::Params> params(
      mail_private::ReadFileToText::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string path = params->path;
  base::FilePath base_path;
  base::FilePath file_path = base_path.AppendASCII(path);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Read, file_path),
      base::BindOnce(&MailPrivateReadFileToTextFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateReadFileToTextFunction::OnFinished(ReadFileResult result) {
  if (result.success == true) {
    Respond(ArgumentList(
        mail_private::ReadFileToText::Results::Create(result.raw)));
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
  std::optional<mail_private::GetFileDirectory::Params> params(
      mail_private::GetFileDirectory::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath::StringType hashed_account_id =
      StringToStringType(params->hashed_account_id);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  file_path = file_path.Append(kMailDirectory);
  file_path = file_path.Append(hashed_account_id);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetDirectory, file_path),
      base::BindOnce(&MailPrivateGetFileDirectoryFunction::OnFinished, this));

  return RespondLater();
}

void MailPrivateGetFileDirectoryFunction::OnFinished(
    GetDirectoryResult result) {
  if (result.success == true) {
    Respond(ArgumentList(mail_private::GetFileDirectory::Results::Create(
        StringTypeToString(result.path))));
  } else {
    Respond(Error("Directory not found"));
  }
}

ExtensionFunction::ResponseAction
MailPrivateCreateFileDirectoryFunction::Run() {
  std::optional<mail_private::CreateFileDirectory::Params> params(
      mail_private::CreateFileDirectory::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string hashed_account_id = params->hashed_account_id;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::FilePath file_path = profile->GetPath();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CreateDirectory, file_path, hashed_account_id),
      base::BindOnce(&MailPrivateCreateFileDirectoryFunction::OnFinished,
                     this));

  return RespondLater();
}

void MailPrivateCreateFileDirectoryFunction::OnFinished(
    GetDirectoryResult result) {
  if (result.success == true) {
    Respond(ArgumentList(mail_private::CreateFileDirectory::Results::Create(
        StringTypeToString(result.path))));
  } else {
    Respond(Error("Directory not created"));
  }
}

mail_client::MessageRow GetMessageRow(const mail_private::Message& message) {
  mail_client::MessageRow row;
  row.searchListId = message.search_list_id;

  row.to = base::UTF8ToUTF16(message.to);
  row.body = base::UTF8ToUTF16(message.body);
  row.subject = base::UTF8ToUTF16(message.subject);
  row.from = base::UTF8ToUTF16(message.from);
  row.cc = base::UTF8ToUTF16(message.cc);
  row.replyTo = base::UTF8ToUTF16(message.reply_to);

  return row;
}

ExtensionFunction::ResponseAction MailPrivateCreateMessagesFunction::Run() {
  std::optional<mail_private::CreateMessages::Params> params(
      mail_private::CreateMessages::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<mail_private::Message>& emails = params->messages;
  size_t count = emails.size();
  EXTENSION_FUNCTION_VALIDATE(count > 0);

  std::vector<mail_client::MessageRow> message_rows;

  for (size_t i = 0; i < count; ++i) {
    mail_private::Message& email_details = emails[i];
    mail_client::MessageRow em = GetMessageRow(email_details);
    message_rows.push_back(em);
  }

  MailClientService* client_service =
      MailClientServiceFactory::GetForProfile(GetProfile());

  client_service->CreateMessages(
      message_rows,
      base::BindOnce(&MailPrivateCreateMessagesFunction::CreateMessagesComplete,
                     this),
      &task_tracker_);

  return RespondLater();  // CreateMessagesComplete() will be called
                          // asynchronously.
}

void MailPrivateCreateMessagesFunction::CreateMessagesComplete(bool result) {
  Respond(ArgumentList(mail_private::CreateMessages::Results::Create(result)));
}

ExtensionFunction::ResponseAction MailPrivateDeleteMessagesFunction::Run() {
  std::optional<mail_private::DeleteMessages::Params> params(
      mail_private::DeleteMessages::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<int>& messages = params->messages;
  mail_client::SearchListIDs messageIds;

  for (size_t i = 0; i < messages.size(); i++) {
    messageIds.push_back(messages[i]);
  }

  MailClientService* service =
      MailClientServiceFactory::GetForProfile(GetProfile());
  service->DeleteMessages(
      messageIds,
      base::BindOnce(&MailPrivateDeleteMessagesFunction::DeleteMessagesComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void MailPrivateDeleteMessagesFunction::DeleteMessagesComplete(bool result) {
  Respond(ArgumentList(mail_private::DeleteMessages::Results::Create(result)));
}

ExtensionFunction::ResponseAction MailPrivateUpdateMessageFunction::Run() {
  std::optional<mail_private::UpdateMessage::Params> params(
      mail_private::UpdateMessage::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  mail_private::Message& email = params->message;
  mail_client::MessageRow messageRow = GetMessageRow(email);

  MailClientService* service =
      MailClientServiceFactory::GetForProfile(GetProfile());

  service->UpdateMessage(
      messageRow,
      base::BindOnce(&MailPrivateUpdateMessageFunction::UpdateMessageComplete,
                     this),
      &task_tracker_);

  return RespondLater();
}

void MailPrivateUpdateMessageFunction::UpdateMessageComplete(
    mail_client::MessageResult results) {
  if (!results.success) {
    Respond(Error(results.message));
  } else {
    Respond(ArgumentList(
        mail_private::UpdateMessage::Results::Create(results.success)));
  }
}

ExtensionFunction::ResponseAction MailPrivateSearchMessagesFunction::Run() {
  std::optional<mail_private::SearchMessages::Params> params(
      mail_private::SearchMessages::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string search = params->search_value;
  std::u16string searchParam = base::UTF8ToUTF16(search);

  MailClientService* service =
      MailClientServiceFactory::GetForProfile(GetProfile());

  service->SearchEmail(
      searchParam,
      base::BindOnce(&MailPrivateSearchMessagesFunction::MessagesSearchComplete,
                     this),
      &task_tracker_);

  return RespondLater();  // MessagesSearchComplete() will be called
                          // asynchronously.
}

void MailPrivateSearchMessagesFunction::MessagesSearchComplete(
    mail_client::SearchListIDs rows) {
  std::vector<double> results;

  for (mail_client::SearchListIDs ::iterator it = rows.begin();
       it != rows.end(); ++it) {
    results.push_back(double(*it));
  }

  Respond(ArgumentList(mail_private::SearchMessages::Results::Create(results)));
}

ExtensionFunction::ResponseAction MailPrivateMatchMessageFunction::Run() {
  std::optional<mail_private::MatchMessage::Params> params(
      mail_private::MatchMessage::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string search = params->search_value;
  std::u16string searchParam = base::UTF8ToUTF16(search);

  mail_client::SearchListID search_list_id = params->search_list_id;

  MailClientService* service =
      MailClientServiceFactory::GetForProfile(GetProfile());

  service->MatchMessage(
      search_list_id, searchParam,
      base::BindOnce(&MailPrivateMatchMessageFunction::MatchMessageComplete,
                     this),
      &task_tracker_);

  return RespondLater();  // MatchMessageComplete() will be called
                          // asynchronously.
}

void MailPrivateMatchMessageFunction::MatchMessageComplete(bool match) {
  Respond(ArgumentList(mail_private::MatchMessage::Results::Create(match)));
}

ExtensionFunction::ResponseAction MailPrivateGetDBVersionFunction::Run() {
  MailClientService* service =
      MailClientServiceFactory::GetForProfile(GetProfile());

  service->GetDBVersion(
      base::BindOnce(&MailPrivateGetDBVersionFunction::OnGetDBVersionFinished,
                     this),
      &task_tracker_);
  return RespondLater();
}

void MailPrivateGetDBVersionFunction::OnGetDBVersionFinished(
    mail_client::Migration migration_row) {
  mail_private::Migration migration;
  migration.db_version = migration_row.db_version;
  migration.migration_needed = migration_row.migration_needed;

  Respond(ArgumentList(mail_private::GetDBVersion::Results::Create(migration)));
}

ExtensionFunction::ResponseAction MailPrivateStartMigrationFunction::Run() {
  MailClientService* service =
      MailClientServiceFactory::GetForProfile(GetProfile());

  service->MigrateSearchDB(
      base::BindOnce(&MailPrivateStartMigrationFunction::OnMigrationFinished,
                     this),
      &task_tracker_);
  return RespondLater();
}

void MailPrivateStartMigrationFunction::OnMigrationFinished(bool success) {
  Respond(ArgumentList(mail_private::StartMigration::Results::Create(success)));
}

ExtensionFunction::ResponseAction MailPrivateDeleteMailSearchDBFunction::Run() {
  MailClientService* service =
      MailClientServiceFactory::GetForProfile(GetProfile());

  service->DeleteMailSearchDB(
      base::BindOnce(&MailPrivateDeleteMailSearchDBFunction::OnDeleteFinished,
                     this),
      &task_tracker_);
  return RespondLater();
}

void MailPrivateDeleteMailSearchDBFunction::OnDeleteFinished(bool success) {
  Respond(
      ArgumentList(mail_private::DeleteMailSearchDB::Results::Create(success)));
}

}  //  namespace extensions

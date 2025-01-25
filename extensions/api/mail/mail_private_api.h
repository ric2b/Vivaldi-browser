// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_MAIL_MAIL_PRIVATE_API_H_
#define EXTENSIONS_API_MAIL_MAIL_PRIVATE_API_H_

#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "components/db/mail_client/mail_client_service.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/mail_private.h"

using mail_client::MailClientService;

namespace extensions {
class MailEventRouter : public mail_client::MailClientModelObserver {
 public:
  explicit MailEventRouter(Profile* profile, MailClientService* mail_service);
  ~MailEventRouter() override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(Profile* profile,
                     const std::string& event_name,
                     base::Value::List event_args);

  // MailClientModelObserver
  void OnMigrationProgress(MailClientService* service,
                           int progress,
                           int total,
                           std::string msg) override;

  void OnDeleteMessagesProgress(MailClientService* service,
                                int delete_progress_count) override;

  const raw_ptr<Profile> profile_;
  base::ScopedObservation<MailClientService,
                          mail_client::MailClientModelObserver>
      mail_service_observation_{this};
};

class MailAPI : public BrowserContextKeyedAPI, public EventRouter::Observer {
 public:
  explicit MailAPI(content::BrowserContext* context);
  ~MailAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<MailAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<MailAPI>;

  const raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "MailAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<MailEventRouter> mail_client_event_router_;
};

class MailPrivateAsyncFunction : public ExtensionFunction {
 public:
  MailPrivateAsyncFunction() = default;

 protected:
  MailClientService* GetMailClientService();
  Profile* GetProfile() const;
  ~MailPrivateAsyncFunction() override {}
};

class MailPrivateGetFilePathsFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.getFilePaths", MAIL_GET_FILE_PATHS)
 public:
  MailPrivateGetFilePathsFunction() = default;

 private:
  ~MailPrivateGetFilePathsFunction() override = default;
  void OnFinished(const std::vector<base::FilePath::StringType>& string_paths);
  void GetFilePaths(bool directory_exists);
  base::FilePath file_path_;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateGetFullPathFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.getFullPath", MAIL_GET_FULL_PATH)
 public:
  MailPrivateGetFullPathFunction() = default;

 private:
  ~MailPrivateGetFullPathFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateGetMailFilePathsFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.getMailFilePaths",
                             MAIL_GET_MAIL_FILE_PATHS)
 public:
  MailPrivateGetMailFilePathsFunction() = default;

 private:
  ~MailPrivateGetMailFilePathsFunction() override = default;
  void OnFinished(const std::vector<base::FilePath::StringType>& string_paths);
  void GetMailFilePaths(bool directory_exists);
  base::FilePath file_path_;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateWriteBufferToMessageFileFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.writeBufferToMessageFile",
                             MAIL_WRITE_BUFFER_TO_MESSAGE_FILE)
 public:
  MailPrivateWriteBufferToMessageFileFunction() = default;

 private:
  ~MailPrivateWriteBufferToMessageFileFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateWriteTextToMessageFileFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.writeTextToMessageFile",
                             MAIL_WRITE_TEXT_TO_MESSAGE_FILE)
 public:
  MailPrivateWriteTextToMessageFileFunction() = default;

 private:
  ~MailPrivateWriteTextToMessageFileFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateDeleteMessageFileFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.deleteMessageFile",
                             MAIL_DELETE_MESSAGE_FILE)
 public:
  MailPrivateDeleteMessageFileFunction() = default;

 private:
  ~MailPrivateDeleteMessageFileFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateRenameMessageFileFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.renameMessageFile",
                             MAIL_RENAME_MESSAGE_FILE)
 public:
  MailPrivateRenameMessageFileFunction() = default;

 private:
  ~MailPrivateRenameMessageFileFunction() override = default;
  void OnFinished(bool result);
  // ExtensionFunction:
  ResponseAction Run() override;
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

 private:
  ~MailPrivateReadFileToBufferFunction() override = default;
  void OnFinished(ReadFileResult result);
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateReadMessageFileToBufferFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.readMessageFileToBuffer",
                             MAIL_READ_MESSAGE_FILE_TO_BUFFER)
 public:
  MailPrivateReadMessageFileToBufferFunction() = default;

 private:
  ~MailPrivateReadMessageFileToBufferFunction() override = default;
  void OnFinished(ReadFileResult result);
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateMessageFileExistsFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.messageFileExists",
                             MAIL_MESSAGE_FILE_EXISTS)
 public:
  MailPrivateMessageFileExistsFunction() = default;

 private:
  ~MailPrivateMessageFileExistsFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateReadFileToTextFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.readFileToText",
                             MAIL_READ_FILE_TO_TEXT)
 public:
  MailPrivateReadFileToTextFunction() = default;

 private:
  ~MailPrivateReadFileToTextFunction() override = default;
  void OnFinished(ReadFileResult result);
  // ExtensionFunction:
  ResponseAction Run() override;
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

 private:
  ~MailPrivateGetFileDirectoryFunction() override = default;
  void OnFinished(GetDirectoryResult result);
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateCreateFileDirectoryFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.createFileDirectory",
                             MAIL_CREATE_FILE_DIRECTORY)
 public:
  MailPrivateCreateFileDirectoryFunction() = default;

 private:
  ~MailPrivateCreateFileDirectoryFunction() override = default;
  void OnFinished(GetDirectoryResult result);
  // ExtensionFunction:
  ResponseAction Run() override;
};

class MailPrivateCreateMessagesFunction : public MailPrivateAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mailPrivate.createMessages", MAIL_CREATE_MESSAGES)
  MailPrivateCreateMessagesFunction() = default;

 private:
  ~MailPrivateCreateMessagesFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the create message function to provide results.
  void CreateMessagesComplete(bool result);

  // The task tracker for the MailClientService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class MailPrivateDeleteMessagesFunction : public MailPrivateAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mailPrivate.deleteMessages", MAIL_DELETE_MESSAGES)
  MailPrivateDeleteMessagesFunction() = default;

 private:
  ~MailPrivateDeleteMessagesFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the DeleteMessages function to provide results.
  void DeleteMessagesComplete(bool result);

  // The task tracker for the MailClientService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class MailPrivateUpdateMessageFunction : public MailPrivateAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mailPrivate.updateMessage", MAIL_UPDATE_MESSAGE)
  MailPrivateUpdateMessageFunction() = default;

 private:
  ~MailPrivateUpdateMessageFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the AddMessageBody function to provide results.
  void UpdateMessageComplete(mail_client::MessageResult result);

  // The task tracker for the MailClientService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class MailPrivateSearchMessagesFunction : public MailPrivateAsyncFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.searchMessages", MAIL_SEARCH_MESSAGES)
 public:
  MailPrivateSearchMessagesFunction() = default;

 private:
  ~MailPrivateSearchMessagesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the MessageSearch function to provide results.
  void MessagesSearchComplete(mail_client::SearchListIDs results);

  base::CancelableTaskTracker task_tracker_;
};

class MailPrivateMatchMessageFunction : public MailPrivateAsyncFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.matchMessage", MAIL_MATCH_MESSAGE)
 public:
  MailPrivateMatchMessageFunction() = default;

 private:
  ~MailPrivateMatchMessageFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the MatchMessage function to provide results.
  void MatchMessageComplete(bool results);

  base::CancelableTaskTracker task_tracker_;
};

class MailPrivateGetDBVersionFunction : public MailPrivateAsyncFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.getDBVersion", MAIL_GET_DB_VERSION)
 public:
  MailPrivateGetDBVersionFunction() = default;

 private:
  ~MailPrivateGetDBVersionFunction() override = default;
  void OnGetDBVersionFinished(mail_client::Migration migration);
  // ExtensionFunction:
  ResponseAction Run() override;
  base::CancelableTaskTracker task_tracker_;
};

class MailPrivateStartMigrationFunction : public MailPrivateAsyncFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.startMigration", MAIL_START_MIGRATION)
 public:
  MailPrivateStartMigrationFunction() = default;

 private:
  ~MailPrivateStartMigrationFunction() override = default;
  void OnMigrationFinished(bool success);
  // ExtensionFunction:
  ResponseAction Run() override;
  base::CancelableTaskTracker task_tracker_;
};

class MailPrivateDeleteMailSearchDBFunction : public MailPrivateAsyncFunction {
  DECLARE_EXTENSION_FUNCTION("mailPrivate.deleteMailSearchDB",
                             MAIL_DELETE_MAIL_SEARCH_DB)
 public:
  MailPrivateDeleteMailSearchDBFunction() = default;

 private:
  ~MailPrivateDeleteMailSearchDBFunction() override = default;
  void OnDeleteFinished(bool success);
  // ExtensionFunction:
  ResponseAction Run() override;
  base::CancelableTaskTracker task_tracker_;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_MAIL_MAIL_PRIVATE_API_H_

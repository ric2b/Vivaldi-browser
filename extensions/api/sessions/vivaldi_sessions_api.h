//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_
#define EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_

#include "browser/sessions/vivaldi_session_utils.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/vivaldi_sessions.h"
#include "sessions/index_model_observer.h"

namespace sessions {
class Index_Model;
}

namespace extensions {

class SessionsPrivateAPI : public BrowserContextKeyedAPI,
                           public ::sessions::IndexModelObserver {
 public:
  explicit SessionsPrivateAPI(content::BrowserContext* context);
  ~SessionsPrivateAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SessionsPrivateAPI>* GetFactoryInstance();

  // ::sessions::IndexModelObserver
  void IndexModelNodeAdded(sessions::Index_Model* model,
                           sessions::Index_Node* node, int64_t parent_id,
                           size_t index, const std::string& owner) override;
  void IndexModelNodeMoved(sessions::Index_Model* model, int64_t id,
                           int64_t parent_id, size_t index) override;
  void IndexModelNodeChanged(sessions::Index_Model* model,
                             sessions::Index_Node* node) override;
  void IndexModelNodeRemoved(sessions::Index_Model* model, int64_t id) override;
  void IndexModelBeingDeleted(sessions::Index_Model* model) override;

  // Functions that will send events to JS.
  static void SendAdded(
    content::BrowserContext* browser_context, sessions::Index_Node* node,
    int parent_id, int index, const std::string& owner);
  static void SendMoved(
    content::BrowserContext* browser_context, int id, int parent_id, int index);
  static void SendChanged(
    content::BrowserContext* browser_context, sessions::Index_Node* node);
  static void SendDeleted(
    content::BrowserContext* browser_context, int id);
  static void SendContentChanged(
    content::BrowserContext* browser_context, int id,
    vivaldi::sessions_private::ContentModel& content_model);
  static void SendOnPersistentLoad(
    content::BrowserContext* browser_context, bool state);

 private:
  friend class BrowserContextKeyedAPIFactory<SessionsPrivateAPI>;
  const raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<sessions::Index_Model> model_ = nullptr;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SessionsPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;
};

class SessionsPrivateAddFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.add", SESSIONS_ADD)
  SessionsPrivateAddFunction();

 private:
  ~SessionsPrivateAddFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  std::optional<vivaldi::sessions_private::Add::Params> params;
  sessions::WriteSessionOptions ctl_;
};

class SessionsPrivateGetAllFunction : public ExtensionFunction,
                                      public ::sessions::IndexModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.getAll", SESSIONS_GETALL)
  SessionsPrivateGetAllFunction() = default;

  // ::sessions::IndexModelObserver
  void IndexModelLoaded(sessions::Index_Model* model) override;

 private:
  ~SessionsPrivateGetAllFunction() override = default;
  void SendResponse(sessions::Index_Model* model);
  void Piggyback();

  // ExtensionFunction:
  ResponseAction Run() override;
};

class SessionsPrivateGetAutosaveIdsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.getAutosaveIds", SESSIONS_GET_AUTOSAVE_IDS)
  SessionsPrivateGetAutosaveIdsFunction() = default;

 private:
  ~SessionsPrivateGetAutosaveIdsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class SessionsPrivateGetContentFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.getContent", SESSIONS_GET_CONTENT)
  SessionsPrivateGetContentFunction() = default;

 private:
  ~SessionsPrivateGetContentFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class SessionsPrivateModifyContentFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.modifyContent", SESSIONS_MODIFY_CONTENT)
  SessionsPrivateModifyContentFunction() = default;

 private:
  ~SessionsPrivateModifyContentFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class SessionsPrivateUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.update", SESSIONS_UPDATE)
  SessionsPrivateUpdateFunction() = default;

 private:
  ~SessionsPrivateUpdateFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};


class SessionsPrivateOpenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.open", SESSIONS_OPEN)
  SessionsPrivateOpenFunction() = default;

 private:
  ~SessionsPrivateOpenFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class SessionsPrivateMakeContainerFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.makeContainer", SESSIONS_MAKE_CONTAINER)
  SessionsPrivateMakeContainerFunction() = default;

 private:
  ~SessionsPrivateMakeContainerFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};


class SessionsPrivateMoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.move", SESSIONS_MOVE)
  SessionsPrivateMoveFunction() = default;

 private:
  ~SessionsPrivateMoveFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};


class SessionsPrivateRenameFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.rename", SESSIONS_RENAME)
  SessionsPrivateRenameFunction() = default;

 private:
  ~SessionsPrivateRenameFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class SessionsPrivateDeleteFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.delete", SESSIONS_DELETE)
  SessionsPrivateDeleteFunction() = default;

 private:
  ~SessionsPrivateDeleteFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class SessionsPrivateEmptyTrashFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.emptyTrash", SESSIONS_EMPTY_TRASH)
  SessionsPrivateEmptyTrashFunction() = default;

 private:
  ~SessionsPrivateEmptyTrashFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class SessionsPrivateRestoreLastClosedFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sessionsPrivate.restoreLastClosed",
                             SESSIONS_REOPEN_LAST)
  SessionsPrivateRestoreLastClosedFunction() = default;

 private:
  ~SessionsPrivateRestoreLastClosedFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SESSIONS_VIVALDI_SESSIONS_API_H_

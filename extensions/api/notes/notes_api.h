// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_NOTES_NOTES_API_H_
#define EXTENSIONS_API_NOTES_NOTES_API_H_

#include <memory>
#include <string>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/notes.h"
#include "notes/notes_model.h"
#include "notes/notes_model_observer.h"
#include "notes/notesnode.h"

using vivaldi::Notes_Model;
using vivaldi::Notes_Node;
using vivaldi::NotesModelObserver;

namespace extensions {

using vivaldi::notes::NoteTreeNode;
using vivaldi::notes::NoteAttachment;

// Observes NotesModel and then routes (some of) the notifications as events to
// the extension system.
class NotesEventRouter : public NotesModelObserver {
 public:
  explicit NotesEventRouter(Profile* profile);
  ~NotesEventRouter() override;

  // vivaldi::NotesModelObserver:
  // TODO(pettern): Wire up the other notifications to send events
  // instead of sending them explicitly.
  void ExtensiveNotesChangesBeginning(Notes_Model* model) override;
  void ExtensiveNotesChangesEnded(Notes_Model* model) override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

  content::BrowserContext* browser_context_;
  Notes_Model* model_;

  DISALLOW_COPY_AND_ASSIGN(NotesEventRouter);
};

class NotesAPI : public BrowserContextKeyedAPI, public EventRouter::Observer {
 public:
  explicit NotesAPI(content::BrowserContext* context);
  ~NotesAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<NotesAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<NotesAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "NotesAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<NotesEventRouter> notes_event_router_;
};

class NotesAsyncFunction : public ChromeAsyncExtensionFunction {
 public:
  Notes_Node* GetNodeFromId(
      Notes_Node* node,
      int64_t id);
  Notes_Model* GetNotesModel();

 protected:
  ~NotesAsyncFunction() override {}
};

class NotesGetFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.get", NOTES_GET)
  NotesGetFunction();

 protected:
  ~NotesGetFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesGetChildrenFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.getChildren", NOTES_GETCHILDREN)
  NotesGetChildrenFunction();

 protected:
  ~NotesGetChildrenFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesGetTreeFunction : public NotesAsyncFunction,
                             public NotesModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.getTree", NOTES_GETTREE)
  NotesGetTreeFunction();

 protected:
  ~NotesGetTreeFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

 private:
  void NotesModelLoaded(Notes_Model* model, bool ids_reassigned) override;
  void NotesModelBeingDeleted(Notes_Model* model) override;

  bool SendGetTreeResponse(Notes_Model* model);
  Notes_Model* notes_model_ = nullptr;
};

class NotesMoveFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.move", NOTES_MOVE)
  NotesMoveFunction();

 protected:
  ~NotesMoveFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesGetSubTreeFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.getSubTree", NOTES_GETSUBTREE)
  NotesGetSubTreeFunction();

 protected:
  ~NotesGetSubTreeFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesSearchFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.search", NOTES_SEARCH)
  NotesSearchFunction();

 protected:
  ~NotesSearchFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesCreateFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.create", NOTES_CREATE)
  NotesCreateFunction();

 protected:
  ~NotesCreateFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesUpdateFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.update", NOTES_UPDATE)
  NotesUpdateFunction();

 protected:
  ~NotesUpdateFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesRemoveFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.remove", NOTES_REMOVE)
  NotesRemoveFunction();

 protected:
  ~NotesRemoveFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesRemoveTreeFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.removeTree", NOTES_REMOVETREE)
  NotesRemoveTreeFunction();

 protected:
  ~NotesRemoveTreeFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

class NotesEmptyTrashFunction : public NotesAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.emptyTrash", NOTES_EMPTYTRASH)
  NotesEmptyTrashFunction();

 protected:
  ~NotesEmptyTrashFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_NOTES_NOTES_API_H_

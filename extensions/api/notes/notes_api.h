// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_NOTES_NOTES_API_H_
#define EXTENSIONS_API_NOTES_NOTES_API_H_

#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "notes/notes_model_observer.h"

namespace extensions {

class NotesAPI : public BrowserContextKeyedAPI,
                 public EventRouter::Observer,
                 public ::vivaldi::NotesModelObserver {
 public:
  explicit NotesAPI(content::BrowserContext* context);
  ~NotesAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<NotesAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // NotesModelObserver implementation.
  void NotesNodeMoved(::vivaldi::NotesModel* model,
                      const ::vivaldi::NoteNode* old_parent,
                      size_t old_index,
                      const ::vivaldi::NoteNode* new_parent,
                      size_t new_index) override;

  void NotesNodeAdded(::vivaldi::NotesModel* model,
                      const ::vivaldi::NoteNode* parent,
                      size_t index) override;
  void NotesNodeRemoved(::vivaldi::NotesModel* model,
                        const ::vivaldi::NoteNode* parent,
                        size_t old_index,
                        const ::vivaldi::NoteNode* node) override;
  void NotesNodeChanged(::vivaldi::NotesModel* model,
                        const ::vivaldi::NoteNode* node) override;
  void ExtensiveNotesChangesBeginning(::vivaldi::NotesModel* model) override;
  void ExtensiveNotesChangesEnded(::vivaldi::NotesModel* model) override;

 private:
  friend class BrowserContextKeyedAPIFactory<NotesAPI>;

  const raw_ptr<content::BrowserContext> browser_context_;

  // Initialized lazily upon the first OnListenerAdded.
  raw_ptr<::vivaldi::NotesModel> model_ = nullptr;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "NotesAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;
};

class NotesGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.get", NOTES_GET)
  NotesGetFunction() = default;

 protected:
  ~NotesGetFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesGetTreeFunction : public ExtensionFunction,
                             public ::vivaldi::NotesModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.getTree", NOTES_GETTREE)
  NotesGetTreeFunction() = default;

 protected:
  ~NotesGetTreeFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

 private:
  void NotesModelLoaded(::vivaldi::NotesModel* model,
                        bool ids_reassigned) override;
  void NotesModelBeingDeleted(::vivaldi::NotesModel* model) override;

  void SendGetTreeResponse(::vivaldi::NotesModel* model);
};

class NotesMoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.move", NOTES_MOVE)
  NotesMoveFunction() = default;

 protected:
  ~NotesMoveFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesSearchFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.search", NOTES_SEARCH)
  NotesSearchFunction() = default;

 protected:
  ~NotesSearchFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesCreateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.create", NOTES_CREATE)
  NotesCreateFunction() = default;

 protected:
  ~NotesCreateFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.update", NOTES_UPDATE)
  NotesUpdateFunction() = default;

 protected:
  ~NotesUpdateFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.remove", NOTES_REMOVE)
  NotesRemoveFunction() = default;

 protected:
  ~NotesRemoveFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesEmptyTrashFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.emptyTrash", NOTES_EMPTYTRASH)
  NotesEmptyTrashFunction() = default;

 protected:
  ~NotesEmptyTrashFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_NOTES_NOTES_API_H_

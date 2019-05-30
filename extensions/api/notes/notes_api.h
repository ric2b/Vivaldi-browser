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

  // vivaldi::NotesModelObserver:
  // TODO(pettern): Wire up the other notifications to send events
  // instead of sending them explicitly.
  void ExtensiveNotesChangesBeginning(::vivaldi::Notes_Model* model) override;
  void ExtensiveNotesChangesEnded(::vivaldi::Notes_Model* model) override;

 private:
  friend class BrowserContextKeyedAPIFactory<NotesAPI>;

  content::BrowserContext* browser_context_;

  // Initialized lazily upon the first OnListenerAdded.
  ::vivaldi::Notes_Model* model_ = nullptr;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "NotesAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;
};

class NotesGetFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.get", NOTES_GET)
  NotesGetFunction() = default;

 protected:
  ~NotesGetFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesGetTreeFunction : public UIThreadExtensionFunction,
                             public ::vivaldi::NotesModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.getTree", NOTES_GETTREE)
  NotesGetTreeFunction() = default;

 protected:
  ~NotesGetTreeFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

 private:
  void NotesModelLoaded(::vivaldi::Notes_Model* model,
                        bool ids_reassigned) override;
  void NotesModelBeingDeleted(::vivaldi::Notes_Model* model) override;

  void SendGetTreeResponse(::vivaldi::Notes_Model* model);
};

class NotesMoveFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.move", NOTES_MOVE)
  NotesMoveFunction() = default;

 protected:
  ~NotesMoveFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesSearchFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.search", NOTES_SEARCH)
  NotesSearchFunction() = default;

 protected:
  ~NotesSearchFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesCreateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.create", NOTES_CREATE)
  NotesCreateFunction() = default;

 protected:
  ~NotesCreateFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesUpdateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.update", NOTES_UPDATE)
  NotesUpdateFunction() = default;

 protected:
  ~NotesUpdateFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesRemoveFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("notes.remove", NOTES_REMOVE)
  NotesRemoveFunction() = default;

 protected:
  ~NotesRemoveFunction() override = default;
  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class NotesEmptyTrashFunction : public UIThreadExtensionFunction {
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

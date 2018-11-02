// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_CONTACTS_CONTACTS_API_H_
#define EXTENSIONS_API_CONTACTS_CONTACTS_API_H_

#include <memory>
#include <string>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/contacts.h"

#include "contact/contact_model_observer.h"
#include "contact/contact_service.h"
#include "contact/contact_type.h"

using contact::ContactModelObserver;
using contact::ContactService;

namespace extensions {

using vivaldi::contacts::Contact;

// Observes ContactModel and then routes (some of) the notifications as
// events to the extension system.
class ContactEventRouter : public ContactModelObserver {
 public:
  explicit ContactEventRouter(Profile* profile);
  ~ContactEventRouter() override;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

  content::BrowserContext* browser_context_;
  ContactService* model_;

  // ContactModelObserver
  void OnContactCreated(ContactService* service,
                        const contact::ContactRow& row) override;
  void OnContactDeleted(ContactService* service,
                        const contact::ContactRow& row) override;
  void OnContactChanged(ContactService* service,
                        const contact::ContactRow& row) override;

  void ExtensiveContactChangesBeginning(ContactService* model) override;
  void ExtensiveContactChangesEnded(ContactService* model) override;

  DISALLOW_COPY_AND_ASSIGN(ContactEventRouter);
};

class ContactsAPI : public BrowserContextKeyedAPI,
                    public EventRouter::Observer {
 public:
  explicit ContactsAPI(content::BrowserContext* context);
  ~ContactsAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ContactsAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<ContactsAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ContactsAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<ContactEventRouter> contact_event_router_;
  DISALLOW_COPY_AND_ASSIGN(ContactsAPI);
};

class ContactAsyncFunction : public UIThreadExtensionFunction {
 public:
  ContactAsyncFunction() = default;

 protected:
  Profile* GetProfile() const;
  ~ContactAsyncFunction() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactAsyncFunction);
};

// Base class for contact funciton APIs which require async interaction with
// chrome services and the extension thread.
class ContactFunctionWithCallback : public ContactAsyncFunction {
 public:
  ContactFunctionWithCallback() = default;

 protected:
  ~ContactFunctionWithCallback() override = default;

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactFunctionWithCallback);
};

class ContactsGetAllFunction : public ContactFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("contacts.getAll", CONTACTS_GETALL)
 public:
  ContactsGetAllFunction() = default;

 protected:
  ~ContactsGetAllFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the contact function to provide results.
  void GetAllComplete(std::shared_ptr<contact::ContactQueryResults> results);

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactsGetAllFunction);
};

class ContactsCreateFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.create", CONTACTS_CREATE)
  ContactsCreateFunction() = default;

 protected:
  ~ContactsCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the contact function to provide results.
  void CreateComplete(std::shared_ptr<contact::CreateContactResult> results);

 private:
  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ContactsCreateFunction);
};

class ContactsUpdateFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.update", CONTACTS_UPDATE)
  ContactsUpdateFunction() = default;

 protected:
  ~ContactsUpdateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateContactComplete(
      std::shared_ptr<contact::UpdateContactResult> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactsUpdateFunction);
};

class ContactsDeleteFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.delete", CONTACTS_DELETE)
  ContactsDeleteFunction() = default;

 protected:
  ~ContactsDeleteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteContactComplete(
      std::shared_ptr<contact::DeleteContactResult> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactsDeleteFunction);
};

class ContactsAddPropertyItemFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.addPropertyItem",
                             CONTACTS_ADD_PROPERTY_ITEM)
  ContactsAddPropertyItemFunction() = default;

 protected:
  ~ContactsAddPropertyItemFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void AddPropertyComplete(
      std::shared_ptr<contact::UpdateContactResult> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactsAddPropertyItemFunction);
};

class ContactsUpdatePropertyItemFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.updatePropertyItem",
                             CONTACTS_UPDATE_PROPERTY_ITEM)
  ContactsUpdatePropertyItemFunction() = default;

 protected:
  ~ContactsUpdatePropertyItemFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdatePropertyComplete(
      std::shared_ptr<contact::UpdateContactResult> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactsUpdatePropertyItemFunction);
};

class ContactsRemovePropertyItemFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.removePropertyItem",
                             CONTACTS_REMOVE_PROPERTY_ITEM)
  ContactsRemovePropertyItemFunction() = default;

 protected:
  ~ContactsRemovePropertyItemFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void RemovePropertyComplete(
      std::shared_ptr<contact::UpdateContactResult> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactsRemovePropertyItemFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_CONTACTS_CONTACTS_API_H_

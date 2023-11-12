// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_CONTACTS_CONTACTS_API_H_
#define EXTENSIONS_API_CONTACTS_CONTACTS_API_H_

#include <memory>
#include <string>

#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/contacts.h"

#include "contact/contact_model_observer.h"
#include "contact/contact_service.h"
#include "contact/contact_type.h"

class Profile;

using contact::ContactModelObserver;
using contact::ContactService;

namespace thunderbirdContacts {
void Read(std::string path, contact::ContactRows& contacts);

}  // namespace thunderbirdContacts

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
                     base::Value::List event_args);

  const raw_ptr<content::BrowserContext> browser_context_;
  const raw_ptr<ContactService> model_;

  // ContactModelObserver
  void OnContactCreated(ContactService* service,
                        const contact::ContactRow& row) override;
  void OnContactDeleted(ContactService* service,
                        const contact::ContactRow& row) override;
  void OnContactChanged(ContactService* service,
                        const contact::ContactRow& row) override;

  void ExtensiveContactChangesBeginning(ContactService* model) override;
  void ExtensiveContactChangesEnded(ContactService* model) override;
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

  void ReadThunderbirdContacts(std::string path,
                               contact::ContactRows& contacts);

 private:
  friend class BrowserContextKeyedAPIFactory<ContactsAPI>;

  const raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ContactsAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<ContactEventRouter> contact_event_router_;
};

class ContactAsyncFunction : public ExtensionFunction {
 public:
  ContactAsyncFunction() = default;

 protected:
  Profile* GetProfile() const;
  ~ContactAsyncFunction() override {}
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
};

class ContactsGetAllFunction : public ContactFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("contacts.getAll", CONTACTS_GETALL)
 public:
  ContactsGetAllFunction() = default;

 private:
  ~ContactsGetAllFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the contact function to provide results.
  void GetAllComplete(std::shared_ptr<contact::ContactQueryResults> results);
};

class ContactsGetAllEmailAddressesFunction
    : public ContactFunctionWithCallback {
  DECLARE_EXTENSION_FUNCTION("contacts.getAllEmailAddresses",
                             CONTACTS_GETALL_EMAILADDRESSES)
 public:
  ContactsGetAllEmailAddressesFunction() = default;

 private:
  ~ContactsGetAllEmailAddressesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the contact function to provide results.
  void GetAllEmailAddressesComplete(
      std::shared_ptr<contact::EmailAddressRows> results);
};

class ContactsCreateFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.create", CONTACTS_CREATE)
  ContactsCreateFunction() = default;

 private:
  ~ContactsCreateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the contact function to provide results.
  void CreateComplete(std::shared_ptr<contact::ContactResults> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsCreateManyFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.createMany", CONTACTS_CREATE_MANY)
  ContactsCreateManyFunction() = default;

 private:
  ~ContactsCreateManyFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback to provide results.
  void CreateManyComplete(
      std::shared_ptr<contact::CreateContactsResult> result);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsUpdateFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.update", CONTACTS_UPDATE)
  ContactsUpdateFunction() = default;

 private:
  ~ContactsUpdateFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateContactComplete(std::shared_ptr<contact::ContactResults> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsDeleteFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.delete", CONTACTS_DELETE)
  ContactsDeleteFunction() = default;

 private:
  ~ContactsDeleteFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void DeleteContactComplete(std::shared_ptr<contact::ContactResults> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsAddPropertyItemFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.addPropertyItem",
                             CONTACTS_ADD_PROPERTY_ITEM)
  ContactsAddPropertyItemFunction() = default;

 private:
  ~ContactsAddPropertyItemFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void AddPropertyComplete(std::shared_ptr<contact::ContactResults> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsUpdatePropertyItemFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.updatePropertyItem",
                             CONTACTS_UPDATE_PROPERTY_ITEM)
  ContactsUpdatePropertyItemFunction() = default;

 private:
  ~ContactsUpdatePropertyItemFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdatePropertyComplete(std::shared_ptr<contact::ContactResults> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsRemovePropertyItemFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.removePropertyItem",
                             CONTACTS_REMOVE_PROPERTY_ITEM)
  ContactsRemovePropertyItemFunction() = default;

 private:
  ~ContactsRemovePropertyItemFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void RemovePropertyComplete(std::shared_ptr<contact::ContactResults> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsAddEmailAddressFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.addEmailAddress", CONTACTS_ADD_EMAIL)
  ContactsAddEmailAddressFunction() = default;

 private:
  ~ContactsAddEmailAddressFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void AddEmailAddressComplete(
      std::shared_ptr<contact::ContactResults> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsRemoveEmailAddressFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.removeEmailAddress",
                             CONTACTS_REMOVE_EMAIL)
  ContactsRemoveEmailAddressFunction() = default;

 private:
  ~ContactsRemoveEmailAddressFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void RemoveEmailAddressComplete(
      std::shared_ptr<contact::ContactResults> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsUpdateEmailAddressFunction : public ContactAsyncFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.updateEmailAddress",
                             CONTACTS_UPDATE_EMAIL)
  ContactsUpdateEmailAddressFunction() = default;

 private:
  ~ContactsUpdateEmailAddressFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  void UpdateEmailAddressComplete(
      std::shared_ptr<contact::ContactResults> results);

  // The task tracker for the ContactService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class ContactsReadThunderbirdContactsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contacts.readThunderbirdContacts",
                             CONTACTS_READ_THUNDERBIRD_CONTACTS)
  ContactsReadThunderbirdContactsFunction() = default;

 private:
  ~ContactsReadThunderbirdContactsFunction() override = default;

  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_CONTACTS_CONTACTS_API_H_

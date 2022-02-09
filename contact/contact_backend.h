// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_BACKEND_H_
#define CONTACT_CONTACT_BACKEND_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/supports_user_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "sql/init_status.h"

#include "contact/contact_backend_notifier.h"
#include "contact/contact_database.h"
#include "contact/contact_type.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace contact {

class ContactDatabase;
struct ContactDatabaseParams;
class ContactBackendHelper;
class ContactDatabase;

// Internal contact implementation which does most of the work of the contact
// system. This runs on a custom created db thread (to not block the browser
// when we do expensive operations) and is NOT threadsafe, so it must only be
// called from message handlers on the background thread.
//
// Most functions here are just the implementations of the corresponding
// functions in the contact service. These functions are not documented
// here, see the contact service for behavior.
class ContactBackend
    : public base::RefCountedThreadSafe<contact::ContactBackend>,
      public ContactBackendNotifier {
 public:
  // Interface implemented by the owner of the ContactBackend object. Normally,
  // the contact service implements this to send stuff back to the main thread.
  // The unit tests can provide a different implementation if they don't have
  // a contact service object.
  class ContactDelegate {
   public:
    virtual ~ContactDelegate() {}

    virtual void NotifyContactCreated(const ContactRow& row) = 0;
    virtual void NotifyContactModified(const ContactRow& row) = 0;
    virtual void NotifyContactDeleted(const ContactRow& row) = 0;

    // Invoked when the backend has finished loading the db.
    virtual void DBLoaded() = 0;
  };

  explicit ContactBackend(ContactDelegate* delegate);

  // This constructor is fast and does no I/O, so can be called at any time.
  ContactBackend(ContactDelegate* delegate,
                 scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Must be called after creation but before any objects are created. If this
  // fails, all other functions will fail as well. (Since this runs on another
  // thread, we don't bother returning failure.)
  //
  // |force_fail| can be set during unittests to unconditionally fail to init.
  void Init(bool force_fail,
            const ContactDatabaseParams& contact_database_params);

  // Notification that the contact system is shutting down. This will break
  // the refs owned by the delegate and any pending transaction so it will
  // actually be deleted.
  void Closing();

  void CancelScheduledCommit();

  void Commit();

  // Creates a Contact
  void CreateContact(ContactRow row, std::shared_ptr<ContactResults> result);
  // Creates multiple contacts
  void CreateContacts(std::vector<ContactRow> contacts,
                      std::shared_ptr<CreateContactsResult> result);
  void GetAllContacts(std::shared_ptr<ContactQueryResults> results);
  void GetAllEmailAddresses(std::shared_ptr<EmailAddressRows> results);
  void UpdateContact(ContactID contact_id,
                     const Contact& contact,
                     std::shared_ptr<ContactResults> result);
  void DeleteContact(ContactID contact_id,
                     std::shared_ptr<ContactResults> result);

  void NotifyContactCreated(const ContactRow& row) override;
  void NotifyContactModified(const ContactRow& row) override;
  void NotifyContactDeleted(const ContactRow& row) override;

  void AddEmailAddress(EmailAddressRow row,
                       std::shared_ptr<ContactResults> result);

  void UpdateEmailAddress(EmailAddressRow row,
                          std::shared_ptr<ContactResults> result);

  void RemoveEmailAddress(ContactID contact_id,
                          EmailAddressID email_id,
                          std::shared_ptr<ContactResults> result);

  void AddProperty(AddPropertyObject row,
                   std::shared_ptr<ContactResults> result);
  void UpdateProperty(UpdatePropertyObject row,
                      std::shared_ptr<ContactResults> result);
  void RemoveProperty(RemovePropertyObject row,
                      std::shared_ptr<ContactResults> result);

 protected:
  ~ContactBackend() override;
  ContactBackend(const ContactBackend&) = delete;
  ContactBackend& operator=(const ContactBackend&) = delete;

 private:
  friend base::RefCountedThreadSafe<ContactBackend>;

  // Does the work of Init.
  void InitImpl(const ContactDatabaseParams& contact_database_params);

  // Closes all databases managed by ContactBackend. Commits any pending
  // transactions.
  void CloseAllDatabases();

  void DeleteEmail(RemovePropertyObject row,
                   std::shared_ptr<ContactResults> result);

  void AddPhoneNumber(AddPropertyObject row,
                      std::shared_ptr<ContactResults> result);
  void UpdatePhonenumber(UpdatePropertyObject row,
                         std::shared_ptr<ContactResults> result);
  void DeletePhonenumber(RemovePropertyObject row,
                         std::shared_ptr<ContactResults> result);

  void AddPostalAddress(AddPropertyObject row,
                        std::shared_ptr<ContactResults> result);
  void UpdatePostalAddress(UpdatePropertyObject row,
                           std::shared_ptr<ContactResults> result);
  void DeletePostalAddress(RemovePropertyObject row,
                           std::shared_ptr<ContactResults> result);

  void FillUpdatedContact(ContactID id, ContactRow& updated_row);

  // Directory where database files will be stored, empty until Init is called.
  base::FilePath contact_dir_;

  // Delegate. See the class definition above for more information. This will
  // be null before Init is called and after Cleanup, but is guaranteed
  // non-null in between.
  std::unique_ptr<ContactDelegate> delegate_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The contact database. Either may be null if the database could
  // not be opened, all users must first check for null and return immediately
  // if it is.
  std::unique_ptr<contact::ContactDatabase> db_;
};

}  // namespace contact

#endif  // CONTACT_CONTACT_BACKEND_H_

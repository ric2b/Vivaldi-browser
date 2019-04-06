// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_SERVICE_H_
#define CONTACT_CONTACT_SERVICE_H_

#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"

#include "contact/contact_backend.h"
#include "contact/contact_database_params.h"
#include "contact/contact_model_observer.h"

class Profile;

namespace base {
class SequencedTaskRunner;
class FilePath;
class Thread;
}  // namespace base

namespace contact {

struct ContactDatabaseParams;
class ContactBackend;

class ContactService : public KeyedService {
 public:
  ContactService();
  ~ContactService() override;

  bool Init(bool no_db, const ContactDatabaseParams& contact_database_params);

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  void AddObserver(ContactModelObserver* observer);
  void RemoveObserver(ContactModelObserver* observer);

  // Call to schedule a given task for running on the contact thread with the
  // specified priority. The task will have ownership taken.
  void ScheduleTask(base::OnceClosure task);

  typedef base::Callback<void(std::shared_ptr<ContactResults>)> ContactCallback;

  typedef base::Callback<void(std::shared_ptr<CreateContactsResult>)>
      CreateContactsCallback;

  typedef base::Callback<void(std::shared_ptr<ContactQueryResults>)>
      QueryContactCallback;

  typedef base::Callback<void(std::shared_ptr<EmailAddressRows>)>
      QueryEmailAddressesCallback;

  base::CancelableTaskTracker::TaskId CreateContact(
      ContactRow ev,
      const ContactCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateContacts(
      std::vector<contact::ContactRow> contacts,
      const CreateContactsCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId AddEmailAddress(
      EmailAddressRow email,
      const ContactCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateEmailAddress(
      EmailAddressRow email,
      const ContactCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId AddProperty(
      AddPropertyObject ev,
      const ContactCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateProperty(
      UpdatePropertyObject ev,
      const ContactCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId RemoveProperty(
      RemovePropertyObject ev,
      const ContactCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllContacts(
      const QueryContactCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllEmailAddresses(
      const QueryEmailAddressesCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateContact(
      ContactID contact_id,
      Contact contact,
      const ContactCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteContact(
      ContactID contact_id,
      const ContactCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Returns true if this contact service is currently in a mode where
  // extensive changes might happen, such as for import and sync. This is
  // helpful for observers that are created after the service has started,
  // and
  // want to check state during their own initializer.
  bool IsDoingExtensiveChanges() const { return extensive_changes_ > 0; }

 private:
  class ContactBackendDelegate;
  friend class base::RefCountedThreadSafe<ContactService>;
  friend class ContactBackendDelegate;
  friend class ContactBackend;

  friend std::unique_ptr<ContactService> CreateContactService(
      const base::FilePath& contact_dir,
      bool create_db);

  void OnDBLoaded();

  // Notify all ContactServiceObservers registered that the
  // ContactService has finished loading.
  void NotifyContactServiceLoaded();
  void NotifyContactServiceBeingDeleted();

  void OnContactCreated(const ContactRow& row);
  void OnContactDeleted(const ContactRow& row);
  void OnContactChanged(const ContactRow& row);

  void Cleanup();

  Profile* profile_;

  base::ThreadChecker thread_checker_;

  // The thread used by the contact service to run ContactBackend operations.
  // Intentionally not a BrowserThread because the sync integration unit tests
  // need to create multiple ContactServices which each have their own thread.
  // Nullptr if TaskScheduler is used for ContactBackend operations.
  std::unique_ptr<base::Thread> thread_;

  // The TaskRunner to which ContactBackend tasks are posted. Nullptr once
  // Cleanup() is called.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // This pointer will be null once Cleanup() has been called, meaning no
  // more calls should be made to the contact thread.
  scoped_refptr<ContactBackend> contact_backend_;

  // Has the backend finished loading? The backend is loaded once Init has
  // completed.
  bool backend_loaded_;

  // The observers.
  base::ObserverList<ContactModelObserver> observers_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_;

  // All vended weak pointers are invalidated in Cleanup().
  base::WeakPtrFactory<ContactService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContactService);
};

}  // namespace contact

#endif  // CONTACT_CONTACT_SERVICE_H_

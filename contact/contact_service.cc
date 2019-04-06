// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/contact_service.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/string_compare.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/variations/variations_associated_data.h"

#include "contact/contact_backend.h"
#include "contact/contact_model_observer.h"
#include "contact/contact_type.h"

using base::Time;

using contact::ContactBackend;
namespace contact {
namespace {

static const char* kContactThreadName = "Vivaldi_ContactThread";
}
// Sends messages from the db backend to us on the main thread. This must be a
// separate class from the contact service so that it can hold a reference to
// the contact service (otherwise we would have to manually AddRef and
// Release when the Backend has a reference to us).

class ContactService::ContactBackendDelegate
    : public ContactBackend::ContactDelegate {
 public:
  ContactBackendDelegate(
      const base::WeakPtr<ContactService>& contact_service,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner)
      : contact_service_(contact_service),
        service_task_runner_(service_task_runner) {}

  void DBLoaded() override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ContactService::OnDBLoaded, contact_service_));
  }

  void NotifyContactCreated(const ContactRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ContactService::OnContactCreated, contact_service_, row));
  }

  void NotifyContactModified(const ContactRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ContactService::OnContactChanged, contact_service_, row));
  }

  void NotifyContactDeleted(const ContactRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ContactService::OnContactDeleted, contact_service_, row));
  }

 private:
  const base::WeakPtr<ContactService> contact_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
};

ContactService::ContactService()
    : thread_(variations::GetVariationParamValue("BrowserScheduler",
                                                 "RedirectContactService") ==
                      "true"
                  ? nullptr
                  : new base::Thread(kContactThreadName)),
      backend_loaded_(false),
      weak_ptr_factory_(this) {}

ContactService::~ContactService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
}

void ContactService::Shutdown() {}

bool ContactService::Init(
    bool no_db,
    const ContactDatabaseParams& contact_database_params) {
  TRACE_EVENT0("browser,startup", "ContactService::Init")
  SCOPED_UMA_HISTOGRAM_TIMER("Contact.ContactServiceInitTime");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!backend_task_runner_);

  if (thread_) {
    base::Thread::Options options;
    options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    if (!thread_->StartWithOptions(options)) {
      Cleanup();
      return false;
    }
    backend_task_runner_ = thread_->task_runner();
  } else {
    backend_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        base::TaskTraits(base::TaskPriority::USER_BLOCKING,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                         base::MayBlock(), base::WithBaseSyncPrimitives()));
  }

  // Create the contact backend.
  scoped_refptr<ContactBackend> backend(new ContactBackend(
      new ContactBackendDelegate(weak_ptr_factory_.GetWeakPtr(),
                                 base::ThreadTaskRunnerHandle::Get()),
      backend_task_runner_));
  contact_backend_.swap(backend);

  ScheduleTask(base::Bind(&ContactBackend::Init, contact_backend_, no_db,
                          contact_database_params));

  return true;
}

void ContactService::ScheduleTask(base::OnceClosure task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(backend_task_runner_);

  backend_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void ContactService::AddObserver(ContactModelObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void ContactService::RemoveObserver(ContactModelObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void ContactService::OnDBLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  backend_loaded_ = true;
  NotifyContactServiceLoaded();
}

void ContactService::NotifyContactServiceLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (ContactModelObserver& observer : observers_)
    observer.OnContactServiceLoaded(this);
}

void ContactService::Cleanup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!backend_task_runner_) {
    // We've already cleaned up.
    return;
  }

  NotifyContactServiceBeingDeleted();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Unload the backend.
  if (contact_backend_.get()) {
    // The backend's destructor must run on the contact thread since it is not
    // threadsafe. So this thread must not be the last thread holding a
    // reference to the backend, or a crash could happen.
    //
    // We have a reference to the contact backend. There is also an extra
    // reference held by our delegate installed in the backend, which
    // ContactBackend::Closing will release. This means if we scheduled a call
    // to ContactBackend::Closing and *then* released our backend reference,
    // there will be a race between us and the backend's Closing function to see
    // who is the last holder of a reference. If the backend thread's Closing
    // manages to run before we release our backend refptr, the last reference
    // will be held by this thread and the destructor will be called from here.
    //
    // Therefore, we create a closure to run the Closing operation first. This
    // holds a reference to the backend. Then we release our reference, then we
    // schedule the task to run. After the task runs, it will delete its
    // reference from the contact thread, ensuring everything works properly.
    //
    contact_backend_->AddRef();
    base::Closure closing_task =
        base::Bind(&ContactBackend::Closing, contact_backend_);
    ScheduleTask(closing_task);
    closing_task.Reset();
    ContactBackend* raw_ptr = contact_backend_.get();
    contact_backend_ = nullptr;
    backend_task_runner_->ReleaseSoon(FROM_HERE, raw_ptr);
  }

  // Clear |backend_task_runner_| to make sure it's not used after Cleanup().
  backend_task_runner_ = nullptr;

  // Join the background thread, if any.
  thread_.reset();
}

void ContactService::NotifyContactServiceBeingDeleted() {
  // TODO(arnar): Add
  /* DCHECK(thread_checker_.CalledOnValidThread());
  for (ContactServiceObserver& observer : observers_)
    observer.ContactServiceBeingDeleted(this);*/
}

void ContactService::OnContactCreated(const ContactRow& row) {
  for (ContactModelObserver& observer : observers_) {
    observer.OnContactCreated(this, row);
  }
}

void ContactService::OnContactDeleted(const ContactRow& row) {
  for (ContactModelObserver& observer : observers_) {
    observer.OnContactDeleted(this, row);
  }
}

void ContactService::OnContactChanged(const ContactRow& row) {
  for (ContactModelObserver& observer : observers_) {
    observer.OnContactChanged(this, row);
  }
}

base::CancelableTaskTracker::TaskId ContactService::CreateContact(
    ContactRow ev,
    const ContactCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::CreateContact, contact_backend_, ev,
                 query_results),
      base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId ContactService::CreateContacts(
    std::vector<contact::ContactRow> contacts,
    const CreateContactsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<CreateContactsResult> create_results =
      std::shared_ptr<CreateContactsResult>(new CreateContactsResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::CreateContacts, contact_backend_, contacts,
                 create_results),
      base::Bind(callback, create_results));
}

base::CancelableTaskTracker::TaskId ContactService::AddProperty(
    AddPropertyObject ev,
    const ContactCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::AddProperty, contact_backend_, ev,
                 query_results),
      base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId ContactService::AddEmailAddress(
    EmailAddressRow email,
    const ContactCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::AddEmailAddress, contact_backend_, email,
                 query_results),
      base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId ContactService::UpdateEmailAddress(
    EmailAddressRow email,
    const ContactCallback& callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::UpdateEmailAddress, contact_backend_, email,
                 query_results),
      base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId ContactService::UpdateProperty(
    UpdatePropertyObject update_property,
    const ContactCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::UpdateProperty, contact_backend_,
                 update_property, query_results),
      base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId ContactService::RemoveProperty(
    RemovePropertyObject ev,
    const ContactCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::RemoveProperty, contact_backend_, ev,
                 query_results),
      base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId ContactService::GetAllContacts(
    const QueryContactCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<ContactQueryResults> query_results =
      std::shared_ptr<ContactQueryResults>(new ContactQueryResults());

  return tracker->PostTaskAndReply(backend_task_runner_.get(), FROM_HERE,
                                   base::Bind(&ContactBackend::GetAllContacts,
                                              contact_backend_, query_results),
                                   base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId ContactService::GetAllEmailAddresses(
    const QueryEmailAddressesCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<contact::EmailAddressRows> query_results =
      std::shared_ptr<EmailAddressRows>(new EmailAddressRows());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::GetAllEmailAddresses, contact_backend_,
                 query_results),
      base::Bind(callback, query_results));
}

base::CancelableTaskTracker::TaskId ContactService::UpdateContact(
    ContactID contact_id,
    Contact contact,
    const ContactCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";

  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<ContactResults> update_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::UpdateContact, contact_backend_, contact_id,
                 contact, update_results),
      base::Bind(callback, update_results));
}

base::CancelableTaskTracker::TaskId ContactService::DeleteContact(
    ContactID contact_id,
    const ContactCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";

  DCHECK(thread_checker_.CalledOnValidThread());

  std::shared_ptr<ContactResults> delete_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&ContactBackend::DeleteContact, contact_backend_, contact_id,
                 delete_results),
      base::Bind(callback, delete_results));
}

}  // namespace contact

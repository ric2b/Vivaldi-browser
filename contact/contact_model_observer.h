// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_MODEL_OBSERVER_H_
#define CONTACT_CONTACT_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

#include "contact_type.h"

namespace contact {

class ContactService;

// Observer for the Contact Model/Service.
class ContactModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the model has finished loading.
  virtual void ContactModelLoaded(ContactService* model) {}

  virtual void ExtensiveContactChangesBeginning(ContactService* service) {}
  virtual void ExtensiveContactChangesEnded(ContactService* service) {}

  // Invoked from the destructor of the ContactService.
  virtual void ContactModelBeingDeleted(ContactService* service) {}

  virtual void OnContactServiceLoaded(ContactService* service) {}

  virtual void OnContactCreated(ContactService* service,
                                const ContactRow& row) {}
  virtual void OnContactDeleted(ContactService* service,
                                const ContactRow& row) {}
  virtual void OnContactChanged(ContactService* service,
                                const ContactRow& row) {}

 protected:
  ~ContactModelObserver() override {}
};

}  // namespace contact

#endif  // CONTACT_CONTACT_MODEL_OBSERVER_H_

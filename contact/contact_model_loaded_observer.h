// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_MODEL_LOADED_OBSERVER_H_
#define CONTACT_CONTACT_MODEL_LOADED_OBSERVER_H_

#include "base/compiler_specific.h"
#include "contact/contact_model_observer.h"
#include "contact/contact_service.h"

class Profile;

namespace contact {

class ContactModelLoadedObserver : public ContactModelObserver {
 public:
  ContactModelLoadedObserver();
  ContactModelLoadedObserver(const ContactModelLoadedObserver&) = delete;
  ContactModelLoadedObserver& operator=(const ContactModelLoadedObserver&) =
      delete;

 private:
  void ContactModelLoaded(ContactService* service) override;
  void ContactModelBeingDeleted(ContactService* service) override;
};

}  // namespace contact

#endif  // CONTACT_CONTACT_MODEL_LOADED_OBSERVER_H_

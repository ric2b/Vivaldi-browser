// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/contact_model_loaded_observer.h"

#include "contact/contact_service.h"

namespace contact {

ContactModelLoadedObserver::ContactModelLoadedObserver() {}

void ContactModelLoadedObserver::ContactModelLoaded(
    contact::ContactService* service) {
  service->RemoveObserver(this);
  delete this;
}

void ContactModelLoadedObserver::ContactModelBeingDeleted(
    contact::ContactService* service) {
  service->RemoveObserver(this);
  delete this;
}

}  // namespace contact

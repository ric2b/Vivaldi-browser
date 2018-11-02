// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_BACKEND_NOTIFIER_H_
#define CONTACT_CONTACT_BACKEND_NOTIFIER_H_

#include "contact_type.h"

namespace contact {

// The ContactBackendNotifier is an interface for forwarding notifications from
// the ContactBackend's client to all the interested observers (in both
// contact and main thread).
class ContactBackendNotifier {
 public:
  ContactBackendNotifier() {}
  virtual ~ContactBackendNotifier() {}

  // Sends notification that |contact| was created
  virtual void NotifyContactCreated(const ContactRow& row) = 0;

  // Sends notification that |contact| has been changed
  virtual void NotifyContactModified(const ContactRow& row) = 0;

  // Sends notification that |contact| was deleted
  virtual void NotifyContactDeleted(const ContactRow& row) = 0;
};

}  // namespace contact

#endif  // CONTACT_CONTACT_BACKEND_NOTIFIER_H_

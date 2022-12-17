// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMPORTER_IMPORTED_CONTACT_ROW_H_
#define IMPORTER_IMPORTED_CONTACT_ROW_H_

#include <string>

// Used as the target for importing contacts from Thunderbird
struct ImportedContact {
public:
  ImportedContact();
  ImportedContact(const ImportedContact&);
  ImportedContact& operator=(const ImportedContact&);

  std::u16string id;
  std::u16string name;
  std::u16string value;
};

#endif  // IMPORTER_IMPORTED_CONTACT_ROW_H_

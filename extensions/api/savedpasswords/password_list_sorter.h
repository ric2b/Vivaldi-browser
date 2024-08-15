// Copyright (c) 2013-2023 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on password_manager::password_list_sorter.h that got moved into
// chromium\ios\chrome\browser\autofill\manual_fill\password_list_sorter.h
// during Chromium 120 upgrade

#ifndef EXTENSIONS_API_SAVEDPASSWORDS_PASSWORD_LIST_SORTER_H_
#define EXTENSIONS_API_SAVEDPASSWORDS_PASSWORD_LIST_SORTER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace extensions {

// Creates key for sorting password or password exception entries. The key is
// eTLD+1 followed by the reversed list of domains (e.g.
// secure.accounts.example.com => example.com.com.example.accounts.secure) and
// the scheme. If |form| is not blocklisted, username, password and federation
// are appended to the key. If not, no further information is added. For Android
// credentials the canocial spec is included.
std::string CreateSortKey(
    const password_manager::CredentialUIEntry& credential);

// Sort entries of |list| based on sort key. The key is the concatenation of
// origin, entry type (non-Android credential, . If a form in |list| is not
// blocklisted, username, password and federation are also included in sort key.
// Forms that only differ by password_form::PasswordForm::Store are merged.
void SortEntriesAndHideDuplicates(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>* list);

}  // namespace extensions

#endif  // EXTENSIONS_API_SAVEDPASSWORDS_PASSWORD_LIST_SORTER_H_

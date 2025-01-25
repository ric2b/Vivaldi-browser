// Copyright (c) 2013-2023 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "password_list_sorter.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace extensions {

namespace {

std::string SplitByDotAndReverse(std::string_view host) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      host, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::reverse(parts.begin(), parts.end());
  return base::JoinString(parts, ".");
}

constexpr char kSortKeyPartsSeparator = ' ';

// The character that is added to a sort key if there is no federation.
// Note: to separate the entries w/ federation and the entries w/o federation,
// this character should be alphabetically smaller than real federations.
constexpr char kSortKeyNoFederationSymbol = '-';

// Symbols to differentiate between passwords and passkeys.
constexpr char kSortKeyPasswordSymbol = 'w';

}  // namespace

std::string CreateSortKey(
    const password_manager::CredentialUIEntry& credential) {
  std::string shown_origin = GetShownOrigin(credential);

  const auto facet_uri = affiliations::FacetURI::FromPotentiallyInvalidSpec(
      credential.GetFirstSignonRealm());

  std::string site_name =
      net::registry_controlled_domains::GetDomainAndRegistry(
          shown_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (site_name.empty())  // e.g. localhost.
    site_name = shown_origin;

  std::string key = site_name + kSortKeyPartsSeparator;

  // Since multiple distinct credentials might have the same site name, more
  // information is added.
  key += SplitByDotAndReverse(shown_origin);

  if (!credential.blocked_by_user) {
    key += kSortKeyPartsSeparator + base::UTF16ToUTF8(credential.username) +
           kSortKeyPartsSeparator + base::UTF16ToUTF8(credential.password);

    key += kSortKeyPartsSeparator;
    if (!credential.federation_origin.IsValid())
      key += credential.federation_origin.host();
    else
      key += kSortKeyNoFederationSymbol;
  }

  // To separate HTTP/HTTPS credentials, add the scheme to the key.
  key += kSortKeyPartsSeparator + GetShownUrl(credential).scheme();

  // Separate passwords from passkeys.
  key += kSortKeyPartsSeparator;
  if (credential.passkey_credential_id.empty()) {
    key += kSortKeyPasswordSymbol;
  } else {
    key += base::UTF16ToUTF8(credential.user_display_name) +
           kSortKeyPartsSeparator +
           base::HexEncode(credential.passkey_credential_id);
  }
  return key;
}

void SortEntriesAndHideDuplicates(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>* list) {
  std::vector<
      std::pair<std::string, std::unique_ptr<password_manager::PasswordForm>>>
      keys_to_forms;
  keys_to_forms.reserve(list->size());
  for (auto& form : *list) {
    std::string key =
        extensions::CreateSortKey(password_manager::CredentialUIEntry(*form));

    keys_to_forms.emplace_back(std::move(key), std::move(form));
  }

  std::sort(keys_to_forms.begin(), keys_to_forms.end());

  list->clear();

  std::string previous_key;
  for (auto& key_to_form : keys_to_forms) {
    if (key_to_form.first != previous_key) {
      list->push_back(std::move(key_to_form.second));
      previous_key = key_to_form.first;
    }
  }
}

}  // namespace extensions

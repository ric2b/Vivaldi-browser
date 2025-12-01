// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/api/doh/doh_functions.h"
#include "extensions/schema/doh_functions.h"

#include <algorithm>
#include <iterator>

#include "base/rand_util.h"
#include "base/values.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "components/country_codes/country_codes.h"
#include "content/public/browser/storage_partition.h"
#include "net/dns/public/doh_provider_entry.h"

/*

chromium/chrome/common/pref_names.h

SecureDnsMode.SECURE
SecureDnsMode.AUTOMATIC
SecureDnsMode.OFF

prefs::kDnsOverHttpsTemplates
// String containing a space-separated list of DNS over HTTPS templates to use
// in secure mode or automatic mode. If no templates are specified in automatic
// mode, we will attempt discovery of DoH servers associated with the configured
// insecure resolvers.
this.setPrefValue('dns_over_https.templates', resolver.value);
this.setPrefValue('dns_over_https.templates', builtInResolver.value);

prefs::kDnsOverHttpsMode
// String specifying the secure DNS mode to use. Any string other than
// "secure" or "automatic" will be mapped to the default "off" mode.

this.setPrefValue('dns_over_https.mode', mode);

SecureDnsHandler::HandleIsValidConfig
CreateSecureDnsSettingDict

E.g. cloudflare
"dns_over_https":{"mode":"secure","templates":"https://chrome.cloudflare-dns.com/dns-query"}

Default OS:
"dns_over_https":{"mode":"automatic","templates":""}

No secure DNS:
"dns_over_https":{"mode":"off","templates":""}

Custom:
"dns_over_https":{"mode":"secure","templates":"https://chromium.dns.nextdns.io/"}

*/

namespace {
bool EntryIsForCountry(const net::DohProviderEntry* entry,
                       country_codes::CountryId country_id) {
  if (entry->display_globally) {
    return true;
  }
  const auto& countries = entry->display_countries;
  bool matches = std::ranges::any_of(
      countries, [country_id](const std::string& country_code) {
        return country_codes::CountryId(country_code) == country_id;
      });

#if DCHECK_IS_ON()
  if (matches) {
    DCHECK(!entry->ui_name.empty());
    DCHECK(!entry->privacy_policy.empty());
  }
#endif

  return matches;
}

}  // namespace

ExtensionFunction::ResponseAction DnsOverHttpsPrivateDataFetcherFunction::Run() {
  // Copied/adapted from ProvidersForCountry() in
  // //chrome/browser/net/secure_dns_util.cc
  country_codes::CountryId country_id = country_codes::GetCurrentCountryID();
  net::DohProviderEntry::List local_providers;
  std::ranges::copy_if(net::DohProviderEntry::GetList(),
                       std::back_inserter(local_providers),
                       [country_id](const net::DohProviderEntry* entry) {
                         return EntryIsForCountry(entry, country_id);
                       });
  // End copy/adapt

  // Copied/adapted from ProvidersForCountry() in
  // chromium/chrome/browser/ui/webui/settings/settings_secure_dns_handler.cc
  namespace Results =
      extensions::vivaldi::dns_over_https_private::DataFetcher::Results;
  using extensions::vivaldi::dns_over_https_private::DohEntry;

  std::vector<DohEntry> resolvers;
  for (const net::DohProviderEntry* entry : local_providers) {
    net::DnsOverHttpsConfig doh_config({entry->doh_server_config});
    base::Value::Dict dict;
    dict.Set("name", entry->ui_name);
    dict.Set("value", doh_config.ToString());
    dict.Set("policy", entry->privacy_policy);
    auto return_entry = DohEntry::FromValue(dict);
    if (return_entry.has_value())
      resolvers.push_back(std::move(return_entry.value()));
  }
  base::RandomShuffle(resolvers.begin(), resolvers.end());
  // End copy/adapt

  return RespondNow(ArgumentList(Results::Create(resolvers)));
}

DnsOverHttpsPrivateConfigTestFunction::
~DnsOverHttpsPrivateConfigTestFunction() {
  if (runner_) {
    runner_.reset();

    namespace Results =
        extensions::vivaldi::dns_over_https_private::ConfigTest::Results;
    Respond(ArgumentList(Results::Create(false)));
  }
}

ExtensionFunction::ResponseAction DnsOverHttpsPrivateConfigTestFunction::Run() {
  using extensions::vivaldi::dns_over_https_private::ConfigTest::Params;

  std::optional<Params> params = Params::Create(args());
  do {
    if (!(params)) {
      this->SetBadMessage();
      return ValidationFailure(this);
    }
  } while (0);

  // Copied/adapted from ProvidersForCountry() in
  // //chrome/browser/net/secure_dns_util.cc
  DCHECK(!runner_);

  const std::string doh_config = params->config;
  std::optional<net::DnsOverHttpsConfig> parsed =
      net::DnsOverHttpsConfig::FromString(doh_config);
  DCHECK(parsed.has_value());  // `doh_config` must be valid.
  if (!parsed.has_value()) {
    namespace Results =
        extensions::vivaldi::dns_over_https_private::ConfigTest::Results;
    return RespondNow(ArgumentList(Results::Create(false)));
  }

  runner_ = chrome_browser_net::secure_dns::MakeProbeRunner(
      std::move(*parsed), base::BindRepeating([](content::BrowserContext *context) {
        return context->GetDefaultStoragePartition()->GetNetworkContext();
      }, browser_context()));
  runner_->RunProbe(base::BindOnce(
      &DnsOverHttpsPrivateConfigTestFunction::TestCompleted,
      scoped_refptr<DnsOverHttpsPrivateConfigTestFunction>(this)));
  // End copy/adapt

  return RespondLater();
}

void DnsOverHttpsPrivateConfigTestFunction::TestCompleted() {
  namespace Results =
      extensions::vivaldi::dns_over_https_private::ConfigTest::Results;

  bool success =
      runner_->result() == chrome_browser_net::DnsProbeRunner::CORRECT;
  runner_.reset();

  Respond(ArgumentList(Results::Create(success)));
}
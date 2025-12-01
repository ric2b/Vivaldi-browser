// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_DOH_DOH_PRIVATE_API_H_
#define EXTENSIONS_API_DOH_DOH_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

#include "chrome/browser/net/dns_probe_runner.h"

class DnsOverHttpsPrivateDataFetcherFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("dnsOverHttpsPrivate.dataFetcher",
                             DNS_OVER_HTTPS_DATA_FETCHER)

  DnsOverHttpsPrivateDataFetcherFunction() = default;

 private:
  ~DnsOverHttpsPrivateDataFetcherFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class DnsOverHttpsPrivateConfigTestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("dnsOverHttpsPrivate.configTest",
                             DNS_OVER_HTTPS_CONFIG_TEST)

  DnsOverHttpsPrivateConfigTestFunction() = default;

 private:
  ~DnsOverHttpsPrivateConfigTestFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void TestCompleted();

  std::unique_ptr<chrome_browser_net::DnsProbeRunner> runner_;
};

#endif  // EXTENSIONS_API_DOH_DOH_PRIVATE_API_H_

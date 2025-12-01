// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIVALDI_STATUS_FACTORY_H_
#define VIVALDI_STATUS_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

template <typename T>
struct DefaultSingletonTraits;

namespace vivaldi_status {

class VivaldiStatus;

class VivaldiStatusFactory : public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiStatus* GetForBrowserContext(content::BrowserContext* context);

  static VivaldiStatus* GetForBrowserContextIfExists(
      content::BrowserContext* context);

  static VivaldiStatusFactory* GetInstance();

  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<VivaldiStatusFactory>;

  VivaldiStatusFactory();
  ~VivaldiStatusFactory() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  //  namespace vivaldi_status

#endif  // VIVALDI_STATUS_FACTORY_H_
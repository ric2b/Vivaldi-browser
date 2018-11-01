// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_VIVALDI_EXTENSIONS_CLIENT_H_
#define EXTENSIONS_VIVALDI_EXTENSIONS_CLIENT_H_

#include <memory>
#include <string>

#include "chrome/common/extensions/chrome_extensions_client.h"
#include "extensions/permissions/vivaldi_api_permissions.h"

namespace extensions {

class VivaldiExtensionsClient : public ChromeExtensionsClient {
 public:
  VivaldiExtensionsClient();
  ~VivaldiExtensionsClient() override;

  void Initialize() override;

  bool IsAPISchemaGenerated(const std::string& name) const override;
  base::StringPiece GetAPISchema(const std::string& name) const override;
  std::unique_ptr<JSONFeatureProviderSource> CreateAPIFeatureSource()
      const override;

  // Get the LazyInstance for ChromeExtensionsClient.
  static ChromeExtensionsClient* GetInstance();
  static void RegisterVivaldiExtensionsClient();

 private:
  friend struct base::LazyInstanceTraitsBase<VivaldiExtensionsClient>;

  const VivaldiAPIPermissions vivaldi_api_permissions_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiExtensionsClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_VIVALDI_EXTENSIONS_CLIENT_H_

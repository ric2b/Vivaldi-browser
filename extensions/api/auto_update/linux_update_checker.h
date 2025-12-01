// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_AUTO_UPDATE_LINUX_UPDATE_CHECKER_H_
#define EXTENSIONS_API_AUTO_UPDATE_LINUX_UPDATE_CHECKER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "base/version.h"
#include "extensions/api/auto_update/auto_update_status.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace extensions {

struct UpdateResult {
  AutoUpdateStatus status;
  std::string version;
  std::string release_notes_url;
};

// Helper class for checking Vivaldi updates on Linux by downloading and parsing
// the Sparkle XML feed from Vivaldi's update servers.
class LinuxUpdateChecker {
 public:
  using UpdateCallback = base::OnceCallback<void(const UpdateResult& result)>;

  LinuxUpdateChecker();
  ~LinuxUpdateChecker();

  // Starts the update check process
  void CheckForUpdates(network::mojom::URLLoaderFactory* url_loader_factory,
                       UpdateCallback callback);

 private:
  // Gets the appropriate update URL based on architecture
  std::string GetUpdateURL();

  // Callback for when URL loading completes
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Parses the Sparkle XML format and extracts version information
  void ParseUpdateXML(const std::string& xml_content);

  // Callback for when XML parsing completes
  void OnXMLParsed(data_decoder::DataDecoder::ValueOrError result);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  UpdateCallback callback_;

  base::WeakPtrFactory<LinuxUpdateChecker> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_API_AUTO_UPDATE_LINUX_UPDATE_CHECKER_H_
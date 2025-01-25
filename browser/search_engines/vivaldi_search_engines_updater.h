// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_VIVALDI_SEARCH_ENGINES_UPDATER_H_
#define BROWSER_VIVALDI_SEARCH_ENGINES_UPDATER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

#include "components/signature/vivaldi_signature.h"

namespace base {
class FilePath;
}  // namespace base
namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace vivaldi {
class SearchEnginesUpdater {
public:
 static void UpdateSearchEngines(scoped_refptr<network::SharedURLLoaderFactory>
                                     url_loader_factory = nullptr);
 static void UpdateSearchEnginesPrompt(
     scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
         nullptr);

private:
 static void Update(
     scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
     SignedResourceUrl url_id,
     std::optional<base::FilePath> dest_path);
 static void OnRequestResponse(std::unique_ptr<network::SimpleURLLoader> guard,
                               const base::FilePath& download_path,
                               std::unique_ptr<std::string> response_body);
};
}
#endif // BROWSER_VIVALDI_SEARCH_ENGINES_UPDATER_H_

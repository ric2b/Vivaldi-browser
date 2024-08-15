// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_VIVALDI_SEARCH_ENGINES_UPDATER_H_
#define BROWSER_VIVALDI_SEARCH_ENGINES_UPDATER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace vivaldi {
class SearchEnginesUpdater {
public:
  static void Update(
      scoped_refptr<network::SharedURLLoaderFactory>
           url_loader_factory = nullptr);

private:
  static void OnRequestResponse(
      std::unique_ptr<network::SimpleURLLoader> guard,
      std::unique_ptr<std::string> response_body);
};
}
#endif // BROWSER_VIVALDI_SEARCH_ENGINES_UPDATER_H_

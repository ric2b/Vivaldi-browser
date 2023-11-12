// Copyright (c) 2022 Vivaldi. All rights reserved.

#include "browser/stats_reporter_impl.h"

#include "chrome/browser/browser_process.h"
#include "components/embedder_support/user_agent_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace vivaldi {
// static
PrefService* StatsReporterImpl::GetLocalState() {
  return g_browser_process->local_state();
}

// static
scoped_refptr<network::SharedURLLoaderFactory>
StatsReporterImpl::GetUrlLoaderFactory() {
  return g_browser_process->shared_url_loader_factory();
}

// static
std::string StatsReporterImpl::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

}  // namespace vivaldi

// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.
//

#import "browser/stats_reporter_impl.h"

#import "base/files/file_path.h"
#import "base/logging.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/web/public/web_client.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace vivaldi {
// static
std::string StatsReporterImpl::GetUserIdFromLegacyStorage() {
  return std::string();
}

// static
base::FilePath StatsReporterImpl::GetReportingDataFileDir() {
  // We can't easily have a shared folder between multiple vivaldi installations
  // on iOS as it would require extra permissions, so just don't try to
  // keep track of multiple installations on the same system.
  return base::FilePath();
}

// static
PrefService* StatsReporterImpl::GetLocalState() {
  return GetApplicationContext()->GetLocalState();
}

//static
scoped_refptr<network::SharedURLLoaderFactory>
StatsReporterImpl::GetUrlLoaderFactory() {
  return GetApplicationContext()->GetSharedURLLoaderFactory();
}

// static
std::string StatsReporterImpl::GetUserAgent() {
  return web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE);
}

// static
std::string StatsReporterImpl::GetClientHints() {
  return "{}";
}

}  // namespace vivaldi
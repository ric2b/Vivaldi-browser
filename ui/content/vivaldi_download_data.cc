// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#include <string>

#include "components/download/public/common/download_url_parameters.h"
#include "ui/content/vivaldi_download_data.h"

/*
 * Code in this file is compiled as part of the content module in chromium.
 */

namespace vivaldi {

// static
const int VivaldiDownloadData::kKey = 0;

// static
void VivaldiDownloadData::Attach(net::URLRequest* request,
                                 download::DownloadUrlParameters* params) {
  auto request_data = std::make_unique<VivaldiDownloadData>();
  request_data->suggested_filename_ = params->suggested_name();
  request->SetUserData(&kKey, std::move(request_data));
}

// static
VivaldiDownloadData* VivaldiDownloadData::Get(const net::URLRequest* request) {
  return static_cast<VivaldiDownloadData*>(request->GetUserData(&kKey));
}

// static
void VivaldiDownloadData::Detach(net::URLRequest* request) {
  request->RemoveUserData(&kKey);
}

}  // namespace vivaldi

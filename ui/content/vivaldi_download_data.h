// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_CONTENT_VIVALDI_DOWNLOAD_DATA_H_
#define UI_CONTENT_VIVALDI_DOWNLOAD_DATA_H_

#include <string>

#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace download {
class DownloadUrlParameters;
}

/*
 * Code in this file is compiled as part of the content module in chromium.
 */

namespace vivaldi {

// This is a UserData::Data that will be attached to a URLRequest as a
// side-channel for passing download parameters.
class CONTENT_EXPORT VivaldiDownloadData : public base::SupportsUserData::Data {
 public:
  ~VivaldiDownloadData() override {}

  static void Attach(net::URLRequest* request,
                     download::DownloadUrlParameters* params);
  static VivaldiDownloadData* Get(const net::URLRequest* request);
  static void Detach(net::URLRequest* request);

  base::string16 suggested_filename() { return suggested_filename_; }

 private:
  static const int kKey;

  base::string16 suggested_filename_;
};

}  // namespace vivaldi

#endif  // UI_CONTENT_VIVALDI_DOWNLOAD_DATA_H_
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_WEB_SOURCE_H_
#define COMPONENTS_DATASOURCE_VIVALDI_WEB_SOURCE_H_

#include <map>
#include <memory>
#include <string>
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"
#include "url/gurl.h"

class Profile;

// Used to serve webui pages for vivaldi
class VivaldiWebSource : public content::URLDataSource {
 public:
  explicit VivaldiWebSource(Profile* profile);
  ~VivaldiWebSource() override;
  VivaldiWebSource(const VivaldiWebSource&) = delete;
  VivaldiWebSource& operator=(const VivaldiWebSource&) = delete;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& path,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool AllowCaching() override;

 private:
  void ExtractRequestTypeAndData(const GURL& path,
                                 std::string& type,
                                 std::string& data);

  base::WeakPtrFactory<VivaldiWebSource> weak_ptr_factory_;
};

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_

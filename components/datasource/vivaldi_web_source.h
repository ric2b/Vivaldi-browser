// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_WEB_SOURCE_H_
#define COMPONENTS_DATASOURCE_VIVALDI_WEB_SOURCE_H_

#include <map>
#include <memory>
#include <string>
#include "base/macros.h"
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

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;
  bool AllowCaching() const override;
  bool ShouldServiceRequest(const GURL& url,
                            content::ResourceContext* resource_context,
                            int render_process_id) const override;

 private:
  void ExtractRequestTypeAndData(const std::string& path,
                                 std::string& type,
                                 std::string& data);

  base::WeakPtrFactory<VivaldiWebSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiWebSource);
};

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_

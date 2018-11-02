// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_
#define COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_

#include <map>
#include <memory>
#include <string>
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/url_data_source.h"
#include "url/gurl.h"

class Profile;

namespace base {
class RefCountedMemory;
}

namespace gfx {
class Image;
}

namespace image_fetcher {
class ImageFetcher;
}

// Handlers for Vivaldi data will inherit and implement this class
// for providing data to the data source.
class VivaldiDataClassHandler {
 public:
  VivaldiDataClassHandler() = default;
  virtual ~VivaldiDataClassHandler() = default;

  // Returns false on early failure, true otherwise. If false is returned,
  // there is no need to call the callback as the callee will handle the failure
  // condition.
  virtual bool GetData(
      Profile* profile,
      const std::string& data_id,
      const content::URLDataSource::GotDataCallback& callback) = 0;
};

class VivaldiDataSource : public content::URLDataSource {
 public:
  explicit VivaldiDataSource(Profile* profile);
  ~VivaldiDataSource() override;

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
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerForRequestPath(
      const std::string& path) const override;

 private:
  void ExtractRequestTypeAndData(const std::string& path,
                                 std::string& type,
                                 std::string& data);
  VivaldiDataClassHandler* GetOrCreateDataClassHandlerForType(
      const std::string& type);

  std::map<std::string, std::unique_ptr<VivaldiDataClassHandler>>
      data_class_handlers_;

  Profile* profile_;

  // Fallback image to send on failure
  scoped_refptr<base::RefCountedMemory> fallback_image_data_;

  base::WeakPtrFactory<VivaldiDataSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiDataSource);
};

#endif  // COMPONENTS_DATASOURCE_VIVALDI_DATA_SOURCE_H_

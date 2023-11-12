// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_THEME_VIVALDI_THEME_DOWNLOAD_H_
#define COMPONENTS_THEME_VIVALDI_THEME_DOWNLOAD_H_

#include <functional>
#include <string>
#include <vector>

#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#include "components/datasource/vivaldi_theme_io.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace vivaldi {

using ThemeDownloadCallback = base::OnceCallback<void(
    std::string theme_id,
    std::unique_ptr<vivaldi_theme_io::ImportError> error)>;

// This is a class to help download and install vivaldi themes.
// It manages downloading a theme from a given url, then passing it on for
// installation.
class VivaldiThemeDownloadHelper {
 public:
  // `callback` owns `this` and the code assumes that until the callback is
  // called this class is alive.
  VivaldiThemeDownloadHelper(
      std::string theme_id,
      GURL url,
      ThemeDownloadCallback callback,
      base::WeakPtr<Profile> browser_context);
  ~VivaldiThemeDownloadHelper();

  class Delegate {
   public:
    virtual void DownloadStarted(const std::string& theme_id) = 0;
    virtual void DownloadProgress(const std::string& theme_id,
                                  uint64_t current) = 0;
    virtual void DownloadCompleted(const std::string& theme_id,
                                   bool success,
                                   std::string error_msg) = 0;
  };

  void DownloadAndInstall();

  // The delegate must be alive at least until the callback is invoked.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 private:
  void OnDownloadCompleted(base::FilePath path);
  void OnDownloadProgress(uint64_t current);
  void SendResult(std::string theme_id,
                  std::unique_ptr<vivaldi_theme_io::ImportError> error);

  GURL url_;
  std::string theme_id_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  ThemeDownloadCallback callback_;
  base::WeakPtr<Profile> profile_;
  raw_ptr<Delegate> delegate_ = nullptr;
  // Temporary downloaded file to delete after import
  base::FilePath temporary_file_;

  base::WeakPtrFactory<VivaldiThemeDownloadHelper> weak_factory_{this};
};

}  // namespace vivaldi

#endif  // COMPONENTS_THEME_VIVALDI_THEME_DOWNLOAD_H_

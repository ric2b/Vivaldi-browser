// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef DIRECT_MATCH_SERVICE_H_
#define DIRECT_MATCH_SERVICE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/direct_match/qwerty_weighted_distance.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_IOS)
class Profile;
#endif

class QwertyWeightedDistance;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace direct_match {
constexpr float kNeighborWeight = 0.7;

class DirectMatchService : public KeyedService {
 public:
  class DirectMatchUnit {
   public:
    DirectMatchUnit(base::Value::Dict* unit);
    DirectMatchUnit(DirectMatchUnit&& unit);
    ~DirectMatchUnit();
    DirectMatchUnit& operator=(DirectMatchUnit&& unit);

    std::string name;
    std::string title;
    std::string click_url;
    std::string target_url;
    std::string image_url;
    size_t match_offset;
    base::Value::List alternative_names;
    base::Value::List blocked_names;
    std::string image_path;
  };
  DirectMatchService();
  ~DirectMatchService() override;
#if BUILDFLAG(IS_IOS)
  void Load(
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory);
#else
  void Load(Profile* profile);
#endif
  const DirectMatchService::DirectMatchUnit* GetDirectMatch(std::string query);
  float GetAcceptableDirectMatchDistance(std::u16string name);

 private:
  void OnDirectMatchDownloadDone(const size_t loader_idx,
                                 std::unique_ptr<std::string> response_body);
  void RunDirectMatchDownload();
  float QwertyWeightedDamerauLevenshtein(std::string name,
                                         std::string typed_text,
                                         bool similarity);
  float BaseDistanceSimilarity(std::string text,
                               std::string typed_text,
                               bool similarity,
                               bool use_damerau_dist);
  float GetDistance(std::string text,
                    std::string typed_text,
                    std::vector<std::vector<float>> substitute_cost,
                    bool use_damerau_dist);
  void InitializeQwertyCosts();
  void HandleIcons();
  void DownloadIcon(GURL url,
                    base::FilePath image_url,
                    base::OnceCallback<void(base::FilePath)> callback,
                    bool path_exist);
  void OnIconDownloadDone(base::FilePath image_path,
                          size_t loader_idx,
                          base::OnceCallback<void(base::FilePath)> callback,
                          std::unique_ptr<std::string> response_body);
  void RemoveUnusedIcons(const std::vector<base::FilePath>& icons);

  static std::string GetIconPath(std::string image_url);

  std::vector<std::unique_ptr<network::SimpleURLLoader>> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::vector<DirectMatchService::DirectMatchUnit> direct_match_units_;
  QwertyWeightedDistance qwerty_weighted_distance_;
  base::FilePath user_data_dir_;
  net::BackoffEntry report_backoff_;
};

}  // namespace direct_match

#endif  // DIRECT_MATCH_SERVICE_H_

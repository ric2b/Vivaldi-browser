// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef DIRECT_MATCH_SERVICE_H_
#define DIRECT_MATCH_SERVICE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
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

    struct DisplayLocations {
      bool address_bar = false;
      bool sd_dialog = false;
    };

    std::string name;
    std::string title;
    std::string redirect_url;
    std::string image_url;
    size_t match_offset;
    base::Value::List alternative_names;
    base::Value::List blocked_names;
    std::string image_path;
    size_t category;
    size_t position;
    DisplayLocations display_locations;
  };

  public:
    class Observer : public base::CheckedObserver {
     public:
      ~Observer() override;
      virtual void OnFinishedDownloadingDirectMatchUnits() {}
      virtual void OnFinishedDownloadingDirectMatchUnitsIcon() {}
    };

  DirectMatchService();
  ~DirectMatchService() override;
#if BUILDFLAG(IS_IOS)
  void Load(
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory);
#else
  void Load(Profile* profile);
#endif
  // Returns the best direct match for query and a bool telling if it is
  // allowed to be the default option (autocompleted) in the address field.
  std::pair<DirectMatchService::DirectMatchUnit*, bool> GetDirectMatch(std::string query);

  // Returns direct match items for provided `category_id` sorted asc by
  // `position` and filtered by `display_locations`.
  std::vector<const DirectMatchService::DirectMatchUnit*>
      GetDirectMatchesForCategory(size_t category_id) const;

  // Returns popular sites from direct match units.
  // These are the same direct match units except only sorted asc by `position`.
  std::vector<const DirectMatchService::DirectMatchUnit*>
      GetPopularSites() const;
  float GetAcceptableDirectMatchDistance(std::u16string name);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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

  // Helper function to filter and sort DirectMatchUnit objects based on a
  // predicate.
  // This function iterates over all DirectMatchUnits, applies the provided
  // predicate to each unit,
  // collects the ones that satisfy the predicate, and
  // then sorts the resulting list by 'position' in ascending order.
  std::vector<const DirectMatchService::DirectMatchUnit*>
      GetMatchingUnits(
          std::function<bool(const DirectMatchUnit&)> predicate) const;

  std::vector<std::unique_ptr<network::SimpleURLLoader>> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::vector<DirectMatchService::DirectMatchUnit> direct_match_units_;
  QwertyWeightedDistance qwerty_weighted_distance_;
  base::FilePath user_data_dir_;
  net::BackoffEntry report_backoff_;

  base::ObserverList<Observer> observers_;
};

}  // namespace direct_match

#endif  // DIRECT_MATCH_SERVICE_H_

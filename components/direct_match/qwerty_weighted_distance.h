// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include <unordered_map>
#include <vector>

class QwertyWeightedDistance {
 public:
  QwertyWeightedDistance(float neighbor_weight);
  ~QwertyWeightedDistance();
  float QwertyWeightedDamerauLevenshtein(std::u16string name,
                                         std::u16string typed_text,
                                         bool similarity);

 private:
  float BaseDistanceSimilarity(std::u16string text,
                               std::u16string typed_text,
                               bool similarity,
                               bool use_damerau_distance);
  float GetDistance(std::u16string text,
                    std::u16string typed_text,
                    std::vector<std::vector<float>> substitute_cost,
                    bool use_damerau_dist);
  void InitializeQwertyCosts();

  std::vector<std::vector<float>> qwerty_cost_;
  float neighbor_weight_;
};

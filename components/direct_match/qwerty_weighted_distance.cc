// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "qwerty_weighted_distance.h"

namespace {
  const std::unordered_map<char, std::vector<char>> neighbors_ = {
    {'q', {'w', 'a'}},
    {'w', {'e', 's', 'a', 'q'}},
    {'e', {'r', 'd', 's', 'w'}},
    {'r', {'t', 'f', 'd', 'e'}},
    {'t', {'y', 'g', 'f', 'r'}},
    {'y', {'u', 'h', 'g', 't'}},
    {'u', {'i', 'j', 'h', 'y'}},
    {'i', {'o', 'k', 'j', 'u'}},
    {'o', {'p', 'l', 'k', 'i'}},
    {'p', {'l', 'o'}},
    {'a', {'q', 'w', 's', 'z'}},
    {'s', {'w', 'e', 'd', 'x', 'z', 'a'}},
    {'d', {'e', 'r', 'f', 'c', 'x', 's'}},
    {'f', {'r', 't', 'g', 'v', 'c', 'd'}},
    {'g', {'t', 'y', 'h', 'b', 'v', 'f'}},
    {'h', {'y', 'u', 'j', 'n', 'b', 'g'}},
    {'j', {'u', 'i', 'k', 'm', 'n', 'h'}},
    {'k', {'i', 'o', 'l', 'm', 'j'}},
    {'l', {'o', 'p', 'k'}},
    {'z', {'a', 's', 'x'}},
    {'x', {'s', 'd', 'c', 'z'}},
    {'c', {'d', 'f', 'v', 'x'}},
    {'v', {'f', 'g', 'b', 'c'}},
    {'b', {'g', 'h', 'n', 'v'}},
    {'n', {'h', 'j', 'm', 'b'}},
    {'m', {'j', 'k', 'n'}},
  };
}

QwertyWeightedDistance::QwertyWeightedDistance(float neighbor_weight):
  neighbor_weight_(neighbor_weight) {
  InitializeQwertyCosts();
}

QwertyWeightedDistance::~QwertyWeightedDistance() = default;

void QwertyWeightedDistance::InitializeQwertyCosts() {
  const int kAlphabetSize = 128;
  // Initialize all to 1
  qwerty_cost_.resize(kAlphabetSize, std::vector<float>(kAlphabetSize, 1));

  for (const auto& entry : neighbors_) {
    char ch = entry.first;
    int ch_code = static_cast<int>(ch);

    for (char neighbor : entry.second) {
      int neighbor_code = static_cast<int>(neighbor);

      // Neighbor weight
      qwerty_cost_[ch_code][neighbor_code] = neighbor_weight_;
      qwerty_cost_[neighbor_code][ch_code] = neighbor_weight_;
    }
  }
}

float QwertyWeightedDistance::QwertyWeightedDamerauLevenshtein(
  std::u16string name, std::u16string typed_text, bool similarity) {
    return BaseDistanceSimilarity(name,  typed_text, similarity, true);
}

float QwertyWeightedDistance::BaseDistanceSimilarity(
  std::u16string text,
  std::u16string typed_text,
  bool similarity,
  bool use_damerau_distance
) {
  if (text == typed_text) {
    return similarity ? 1 : 0;
  }
  float distance = GetDistance(
    text, typed_text, qwerty_cost_, use_damerau_distance);
  if (!similarity) {
    return distance;
  }

  size_t text_len = text.size();
  size_t typed_text_len = typed_text.size();
  float max_dist = static_cast<float>(std::max(text_len, typed_text_len));
  return (max_dist - distance) / max_dist;
}

float QwertyWeightedDistance::GetDistance(
    std::u16string text,
    std::u16string typed_text,
    std::vector<std::vector<float>> sub_cost,
    bool use_damerau_distance
) {
    int text_len = static_cast<int>(text.size());
    int typed_text_len = static_cast<int>(typed_text.size());

    // Create a 2D vector to store the distances
    std::vector<std::vector<float>> distance_array(
      text_len + 1, std::vector<float>(typed_text_len + 1, 0));

    // Initialize the first row and column
    for (int i = 0; i <= text_len; ++i) {
      distance_array[i][0] = i;
    }

    for (int j = 0; j <= typed_text_len; ++j) {
      distance_array[0][j] = j;
    }

    for (int i = 1; i <= text_len; ++i) {
      for (int j = 1; j <= typed_text_len; ++j) {
        const bool use_sub_cost = text[i - 1] != typed_text[j - 1];
        float cost = 0;
        if (use_sub_cost) {
          const int sub_cost_text_ch_code = static_cast<int>(text[i - 1]);
          const int sub_cost_typed_ch_code = static_cast<int>(typed_text[j - 1]);
          if (sub_cost_text_ch_code < static_cast<int>(sub_cost.size()) &&
              sub_cost_typed_ch_code <
                static_cast<int>(sub_cost[sub_cost_text_ch_code].size())
          ) {
            cost = sub_cost[sub_cost_text_ch_code][sub_cost_typed_ch_code];
          } else {
            cost = 1;
          }
        }
        distance_array[i][j] = std::min({
          distance_array[i - 1][j] + 1,     // Deletion
          distance_array[i][j - 1] + 1,     // Insertion
          distance_array[i - 1][j - 1] + cost   // Substitution
        });

        if (use_damerau_distance &&
            i > 1 &&
            j > 1 &&
            text[i - 1] == typed_text[j - 2] &&
            text[i - 2] == typed_text[j - 1]) {
          distance_array[i][j] =
            std::min(distance_array[i][j],
                      distance_array[i - 2][j - 2] + cost);  // Transposition
        }
      }
    }

    return distance_array[text_len][typed_text_len];
}

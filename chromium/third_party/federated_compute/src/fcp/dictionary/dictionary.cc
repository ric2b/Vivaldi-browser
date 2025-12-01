/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "fcp/dictionary/dictionary.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "fcp/base/monitoring.h"
#include "fcp/dictionary/dictionary.pb.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace fcp {
namespace dictionary {

// Bidirectional map defined as hash_map from strings to int32_t paired with
// a vector of those keys for reverse lookup.
typedef std::pair<absl::node_hash_map<std::string, int32_t>,
                  std::vector<std::string>>
    HashVectorBimap;

namespace {

// Map a string to an ID, using a bidirectional map (an std::pair containing
// two data structures for string -> int and for int -> string lookups).
int32_t MapLookup(const HashVectorBimap& bimap, const absl::string_view tag) {
  auto map_idx = bimap.first.find(tag);
  return map_idx == bimap.first.end() ? Dictionary::kNotFound : map_idx->second;
}
// Lookup a token given its ID.
std::string MapReverseLookup(const HashVectorBimap& bimap, int32_t id) {
  if (id < 0 || id >= bimap.second.size()) {
    return "";
  }
  return bimap.second[id];
}

// Return the size of an stl-like data structure.
int32_t GetSize(const HashVectorBimap& bimap) {
  return static_cast<int32_t>(bimap.first.size());
}

int32_t GetMaxSpecialId(const DictionaryDescription::SpecialIds& special_ids) {
  int32_t max_special_id = -1;
  max_special_id = std::max(max_special_id, special_ids.bos());
  max_special_id = std::max(max_special_id, special_ids.eos());
  max_special_id = std::max(max_special_id, special_ids.unk());
  return max_special_id;
}

// Dictionary implementation powered by templated utility functions above.
template <typename Bimap>
class DictionaryImpl : public Dictionary {
 public:
  DictionaryImpl(
      std::unique_ptr<Bimap> bimap,
      const DictionaryDescription::SpecialIds& special_ids,
      const DictionaryDescription::OutputBlocklistIds& output_blocklist_ids)
      : bimap_(std::move(bimap)),
        special_ids_(special_ids),
        max_special_id_(GetMaxSpecialId(special_ids)) {
    // Validate special ids.
    FCP_CHECK(special_ids.has_bos() == (special_ids.bos() >= 0));
    FCP_CHECK(special_ids.has_eos() == (special_ids.eos() >= 0));
    FCP_CHECK(special_ids.has_unk() == (special_ids.unk() >= 0));

    // Token numbering starts at max(special_ids) + 1.
    output_blocklist_ids_.reserve(max_special_id_ + 1 +
                                  output_blocklist_ids.id_size());
    for (int32_t id = 0; id <= max_special_id_; ++id) {
      output_blocklist_ids_.push_back(id);
    }
    for (int32_t id : output_blocklist_ids.id()) {
      output_blocklist_ids_.push_back(id);
    }
  }

  int32_t Size() const override {
    return GetSize(*bimap_) + max_special_id_ + 1;
  }

  int32_t TokenToId(const absl::string_view tag) const override {
    int32_t id = MapLookup(*bimap_, tag);
    if (id == kNotFound) {
      return special_ids_.unk();
    } else {
      return id + max_special_id_ + 1;
    }
  }

  std::string IdToToken(int32_t id) const override {
    return MapReverseLookup(*bimap_, id - (max_special_id_ + 1));
  }

  bool IsSpecialId(int32_t token_id) const override {
    return token_id <= max_special_id_;
  }

  const std::vector<int32_t>& GetSortedOutputBlocklistIds() const override {
    return output_blocklist_ids_;
  }

  const DictionaryDescription::SpecialIds& GetSpecialIds() const override {
    return special_ids_;
  }

 private:
  const std::unique_ptr<Bimap> bimap_;
  const DictionaryDescription::SpecialIds special_ids_;
  int32_t max_special_id_;
  std::vector<int32_t> output_blocklist_ids_;
};

absl::Status IsOutputBlocklistIdsSortedAndUnique(
    const DictionaryDescription& description) {
  // All blocklist ids must be greater than max_special_id.
  const int32_t max_special_id = GetMaxSpecialId(description.special_ids());

  // Make sure output blocklist IDs are sorted in ascending order and unique.
  if (description.has_output_blocklist_ids()) {
    for (int i = 0; i < description.output_blocklist_ids().id_size(); i++) {
      if (description.output_blocklist_ids().id(i) <= max_special_id) {
        return absl::InvalidArgumentError(
            "output_blocklist_ids should not overlap with special ids");
      }
      if (!(i == 0 || description.output_blocklist_ids().id(i) >
                          description.output_blocklist_ids().id(i - 1))) {
        return absl::InvalidArgumentError(
            "output_blocklist_ids not unique or sorted");
      }
    }
  }
  return absl::OkStatus();
}

}  // anonymous namespace

absl::StatusOr<std::unique_ptr<Dictionary>> Dictionary::Create(
    const DictionaryDescription& description) {
  if (!description.has_vocabulary()) {
    return absl::InvalidArgumentError(
        "Cannot create a dictionary that does not have vocabulary set");
  }
  // Make sure output blocklist IDs are sorted in ascending order and unique.
  FCP_RETURN_IF_ERROR(IsOutputBlocklistIdsSortedAndUnique(description));

  if (description.vocabulary().has_index()) {
    auto bimap = std::make_unique<HashVectorBimap>();
    int i = 0;
    bimap->second.reserve(description.vocabulary().index().token_size());
    for (absl::string_view token : description.vocabulary().index().token()) {
      FCP_CHECK(!token.empty());
      bimap->first[token] = i++;
      bimap->second.emplace_back(token);
    }
    return std::unique_ptr<Dictionary>(new DictionaryImpl<HashVectorBimap>(
        std::move(bimap), description.special_ids(),
    description.output_blocklist_ids()));
  } else {
    return absl::InvalidArgumentError(
        "Invalid DictionaryDescription: no vocabulary specified.");
  }
}
}  // namespace dictionary
}  // namespace fcp

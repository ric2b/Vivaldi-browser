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
#ifndef FCP_DICTIONARY_DICTIONARY_H_
#define FCP_DICTIONARY_DICTIONARY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "fcp/dictionary/dictionary.pb.h"

namespace fcp {
namespace dictionary {

// Interface for mapping tokens (usually words) to indices.
class Dictionary {
 public:
  virtual ~Dictionary() = default;

  // Returns the number of elements in the dictionary.
  virtual int32_t Size() const = 0;

  // Returns the index of token in the dictionary or kNotFound if not found.
  virtual int32_t TokenToId(absl::string_view token) const = 0;

  // Maps an ID to a string if the ID represents a valid token.
  // Returns "" on error.
  virtual std::string IdToToken(int32_t id) const = 0;

  // Returns true if the given id is set via DictionaryDescription.SpecialIds.
  virtual bool IsSpecialId(int32_t id) const = 0;

  // Returns a sorted (ascending) list of ids to filter from the predictions.
  // Can be used for e.g. punctuation. Includes special ids.
  virtual const std::vector<int32_t>& GetSortedOutputBlocklistIds() const = 0;

  // Returns the special ids used in this dictionary.
  virtual const DictionaryDescription::SpecialIds& GetSpecialIds() const = 0;

  // Id returned when an element is not found. This is distinct from the id
  // of the unknown_token (if one is configured).
  static constexpr int32_t kNotFound = -1;

  //
  // Static constructors
  //

  // Creates a dictionary from a self-describing DictionaryDescription proto.
  static absl::StatusOr<std::unique_ptr<Dictionary>> Create(
      const DictionaryDescription& description);
};

}  // namespace dictionary
}  // namespace fcp

#endif  // FCP_DICTIONARY_DICTIONARY_H_

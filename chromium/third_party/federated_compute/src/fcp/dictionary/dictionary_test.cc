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

#include <memory>
#include <string>
#include <vector>

#include "fcp/base/monitoring.h"
#include "fcp/dictionary/dictionary.pb.h"
#include "fcp/testing/parse_text_proto.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace fcp {
namespace dictionary {

using ::testing::ElementsAre;

class DictionaryTest : public ::testing::Test {};

TEST_F(DictionaryTest, TestMapDictionaryLookup) {
  std::unique_ptr<Dictionary> dictionary = *Dictionary::Create(PARSE_TEXT_PROTO(
      "vocabulary: < index: < token: 'a' token: 'b' token: 'c' > >"));

  EXPECT_EQ(0, dictionary->TokenToId("a"));
  EXPECT_EQ(1, dictionary->TokenToId("b"));
  EXPECT_EQ(2, dictionary->TokenToId("c"));
  EXPECT_EQ(Dictionary::kNotFound, dictionary->TokenToId("d"));
}

TEST_F(DictionaryTest, TestMapDictionaryLookupWithUnk) {
  std::unique_ptr<Dictionary> dictionary = *Dictionary::Create(
      PARSE_TEXT_PROTO("special_ids: < unk: 0 bos: 1 > "
                       "vocabulary: < index: <"
                       "  token: 'a' token: 'b' token: 'c' > >"));
  EXPECT_EQ(2, dictionary->TokenToId("a"));
  EXPECT_EQ(3, dictionary->TokenToId("b"));
  EXPECT_EQ(4, dictionary->TokenToId("c"));
  EXPECT_EQ(0, dictionary->TokenToId("d"));
  EXPECT_EQ(0, dictionary->TokenToId("e"));
  EXPECT_EQ(0, dictionary->TokenToId("<UNK>"));
  EXPECT_EQ(0, dictionary->TokenToId("<BOS>"));
}

TEST_F(DictionaryTest, TestMapDictionaryLookupWithSpecialTokenHoles) {
  std::unique_ptr<Dictionary> dictionary = *Dictionary::Create(
      PARSE_TEXT_PROTO("special_ids: < unk: 1 bos: 4 > "
                       "vocabulary: < index: <"
                       "  token: 'a' token: 'b' token: 'c' > >"));

  // Make sure dictionary doesn't use the "holes" in IDs - 0, 2 and 3 - for
  // tokens, but starts numbering tokens with max(special_ids) + 1.
  EXPECT_EQ(5, dictionary->TokenToId("a"));
  EXPECT_EQ(6, dictionary->TokenToId("b"));
  EXPECT_EQ(7, dictionary->TokenToId("c"));
  EXPECT_EQ(1, dictionary->TokenToId("d"));
  EXPECT_EQ(1, dictionary->TokenToId("e"));
  EXPECT_EQ(1, dictionary->TokenToId("<UNK>"));
  EXPECT_EQ(1, dictionary->TokenToId("<BOS>"));
}

TEST_F(DictionaryTest, TestMapDictionaryReverseLookup) {
  std::unique_ptr<Dictionary> dictionary = *Dictionary::Create(PARSE_TEXT_PROTO(
      "vocabulary: < index: < token: 'a' token: 'b' token: 'c' > >"));
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("a")), "a");
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("b")), "b");
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("c")), "c");
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("d")), "");
  EXPECT_EQ(dictionary->IdToToken(0xDEADBEEF), "");
  EXPECT_EQ(dictionary->IdToToken(1337), "");
}

TEST_F(DictionaryTest, TestMapDictionaryReverseLookupWithUnk) {
  std::unique_ptr<Dictionary> dictionary = *Dictionary::Create(
      PARSE_TEXT_PROTO("special_ids: < unk: 0 bos: 1 > "
                       "vocabulary: < index: <"
                       "  token: 'a' token: 'b' token: 'c' > >"));
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("a")), "a");
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("b")), "b");
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("c")), "c");
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("d")), "");
  EXPECT_EQ(dictionary->IdToToken(0xDEADBEEF), "");
  EXPECT_EQ(dictionary->IdToToken(1337), "");
}

TEST_F(DictionaryTest, TestMapDictionaryReverseLookupWithSpecialTokenHoles) {
  std::unique_ptr<Dictionary> dictionary = *Dictionary::Create(
      PARSE_TEXT_PROTO("special_ids: < unk: 1 bos: 4 > "
                       "vocabulary: < index: <"
                       "  token: 'a' token: 'b' token: 'c' > >"));
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("a")), "a");
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("b")), "b");
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("c")), "c");
  EXPECT_EQ(dictionary->IdToToken(dictionary->TokenToId("d")), "");
  EXPECT_EQ(dictionary->IdToToken(0xDEADBEEF), "");
  EXPECT_EQ(dictionary->IdToToken(1337), "");
}
}  // namespace dictionary
}  // namespace fcp

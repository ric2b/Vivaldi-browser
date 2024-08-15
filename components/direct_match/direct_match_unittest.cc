// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "direct_match_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace direct_match {

TEST(DirectMatchTest, QwertyWeightedDamerauLevenshteinDistance) {
  QwertyWeightedDistance weighted_distance(direct_match::kNeighborWeight);
  float dist;

  // Distance
  dist = weighted_distance.QwertyWeightedDamerauLevenshtein(u"hello", u"hello",
                                                            false);
  EXPECT_EQ(dist, 0);

  dist = weighted_distance.QwertyWeightedDamerauLevenshtein(u"kitten",
                                                            u"sitting", false);
  EXPECT_EQ(dist, 3);

  dist = weighted_distance.QwertyWeightedDamerauLevenshtein(u"", u"", false);
  EXPECT_EQ(dist, 0);

  dist =
      weighted_distance.QwertyWeightedDamerauLevenshtein(u"", u"hello", false);
  EXPECT_EQ(dist, 5);

  dist =
      weighted_distance.QwertyWeightedDamerauLevenshtein(u"hello", u"", false);
  EXPECT_EQ(dist, 5);

  dist = weighted_distance.QwertyWeightedDamerauLevenshtein(
      u"hello world", u"hella wrld", false);
  EXPECT_EQ(dist, 2);

  // QwertyWeightedDamerauLevenshtein doesn't lower string
  // it must be done before.
  dist = weighted_distance.QwertyWeightedDamerauLevenshtein(u"HELLO", u"HeLlo",
                                                            false);
  EXPECT_EQ(dist, 3);

  dist =
      weighted_distance.QwertyWeightedDamerauLevenshtein(u"ps", u"pw", false);
  EXPECT_EQ(dist, direct_match::kNeighborWeight);

  dist = weighted_distance.QwertyWeightedDamerauLevenshtein(
      u"hello world", u"hello wrold", false);
  EXPECT_EQ(dist, 1);

  dist = weighted_distance.QwertyWeightedDamerauLevenshtein(
      u"hello world", u"hello wreld", false);
  EXPECT_EQ(dist, 1 + direct_match::kNeighborWeight);

  // Similarity
  dist = weighted_distance.QwertyWeightedDamerauLevenshtein(u"hello", u"hella",
                                                            true);
  EXPECT_EQ(dist, 0.8f);
}

float GetNumberOfNeighborMistake(float distance, float neighbor_weight) {
  return std::floor(distance / neighbor_weight);
}

float GetNumberOfMistake(float distance) {
  return std::floor(distance);
}

TEST(DirectMatchTest, AcceptableDistance) {
  DirectMatchService* service = new DirectMatchService();
  const float neighbor_weight = direct_match::kNeighborWeight;

  float dist;
  // 1 typed character
  dist = service->GetAcceptableDirectMatchDistance(u"a");
  EXPECT_EQ(GetNumberOfMistake(dist), 0);
  EXPECT_EQ(GetNumberOfNeighborMistake(dist, neighbor_weight), 0);

  // 2 typed characters
  dist = service->GetAcceptableDirectMatchDistance(u"ab");
  EXPECT_EQ(GetNumberOfMistake(dist), 0);
  EXPECT_EQ(GetNumberOfNeighborMistake(dist, neighbor_weight), 0);

  // 3 typed characters
  dist = service->GetAcceptableDirectMatchDistance(u"abc");
  EXPECT_EQ(GetNumberOfMistake(dist), 0);
  EXPECT_EQ(GetNumberOfNeighborMistake(dist, neighbor_weight), 1);

  // 6 typed characters
  dist = service->GetAcceptableDirectMatchDistance(u"abcdef");
  EXPECT_EQ(GetNumberOfMistake(dist), 1);
  EXPECT_EQ(GetNumberOfNeighborMistake(dist, neighbor_weight), 2);

  // 10 typed characters
  dist = service->GetAcceptableDirectMatchDistance(u"abcdefghij");
  EXPECT_EQ(GetNumberOfMistake(dist), 2);
  EXPECT_EQ(GetNumberOfNeighborMistake(dist, neighbor_weight), 3);

  // 12 typed characters
  dist = service->GetAcceptableDirectMatchDistance(u"abcdefghijkl");
  EXPECT_EQ(GetNumberOfMistake(dist), 2);
  EXPECT_EQ(GetNumberOfNeighborMistake(dist, neighbor_weight), 3);
}

}  // namespace direct_match

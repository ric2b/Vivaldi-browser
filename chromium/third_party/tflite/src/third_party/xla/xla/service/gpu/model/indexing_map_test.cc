/* Copyright 2023 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xla/service/gpu/model/indexing_map.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/hash/hash_testing.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "mlir/IR/AffineExpr.h"  // from @llvm-project
#include "mlir/IR/AffineMap.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/service/gpu/model/affine_map_printer.h"
#include "xla/service/gpu/model/indexing_test_utils.h"
#include "xla/tests/hlo_test_base.h"
#include "xla/tests/verified_hlo_module.h"
#include "tsl/platform/statusor.h"
#include "tsl/platform/test.h"

namespace xla {
namespace gpu {
namespace {

using ::mlir::AffineMap;
using ::testing::AnyOf;
using ::testing::ElementsAre;

class IndexingMapTest : public HloTestBase {
 public:
  mlir::MLIRContext mlir_context_;
  AffineMapPrinter printer_;
};

std::vector<bool> ConvertToSTL(const llvm::SmallBitVector& bit_vector) {
  std::vector<bool> result;
  result.reserve(bit_vector.size());
  for (int i = 0; i < bit_vector.size(); ++i) {
    result.push_back(bit_vector[i]);
  }
  return result;
}

TEST_F(IndexingMapTest, RTVar) {
  auto zero_dim_map = AffineMap::get(&mlir_context_);
  std::vector<RTVar> rt_vars{RTVar{Interval{0, 2},
                                   /*instr=*/nullptr, zero_dim_map},
                             RTVar({Interval{0, 7},
                                    /*instr=*/nullptr, zero_dim_map})};

  IndexingMap indexing_map(
      ParseAffineMap("(d0, d1)[s0, s1, s2] -> (d1, d0, s0 + s1, s1)",
                     &mlir_context_),
      {DimVar{{0, 99}}, DimVar{{0, 43}}}, {RangeVar{{-99, 99}}},
      std::move(rt_vars));
  printer_.SetSymbolName(0, "range");
  printer_.SetSymbolName(1, "rt_0");
  printer_.SetSymbolName(2, "rt_1");
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0, d1)[range, rt_0, rt_1] -> (d1, d0, range + rt_0, rt_0)
              domain:
              d0 in [0, 100)
              d1 in [0, 44)
              range in [-99, 100)
              rt_0 in [0, 3)
                hlo: NULL
                () -> ()
              rt_1 in [0, 8)
                hlo: NULL
                () -> ()
              )"));
}

TEST_F(IndexingMapTest, Evaluation) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1, s0)", &mlir_context_),
      {4, 4}, {2, 2});

  auto results = indexing_map.Evaluate(
      mlir::getAffineConstantExprs({1, 2}, &mlir_context_),
      mlir::getAffineConstantExprs({3, 4}, &mlir_context_));
  EXPECT_THAT(results, ElementsAre(2, 1, 4, 3));

  auto feasible = indexing_map.ConstraintsSatisfied(
      mlir::getAffineConstantExprs({1, 2}, &mlir_context_),
      mlir::getAffineConstantExprs({3, 4}, &mlir_context_));
  EXPECT_TRUE(feasible);

  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 4", &mlir_context_),
                             Interval{0, 0});

  auto infeasible = indexing_map.ConstraintsSatisfied(
      mlir::getAffineConstantExprs({1, 2}, &mlir_context_),
      mlir::getAffineConstantExprs({5, 4}, &mlir_context_));
  EXPECT_FALSE(infeasible);
}

TEST_F(IndexingMapTest, Composition_Permutation) {
  IndexingMap producer = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1, s0)", &mlir_context_),
      {4, 4}, {2, 2});

  IndexingMap consumer = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_), {4}, {4});

  auto composed = ComposeIndexingMaps(consumer, producer);
  EXPECT_THAT(composed, MatchIndexingMap(R"(
                          (d0)[s0, s1, s2] -> (s2, d0, s1, s0)
                          domain:
                          d0 in [0, 4)
                          s0 in [0, 2)
                          s1 in [0, 2)
                          s2 in [0, 4)
                        )"));
}

TEST_F(IndexingMapTest, Composition_RestrictedInterval) {
  IndexingMap producer = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1, s0)", &mlir_context_),
      {5, 6}, {7, 2});

  IndexingMap consumer = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_), {10}, {8});

  auto composed = ComposeIndexingMaps(consumer, producer);
  EXPECT_THAT(composed, MatchIndexingMap(R"(
                          (d0)[s0, s1, s2] -> (s2, d0, s1, s0)
                          domain:
                          d0 in [0, 5)
                          s0 in [0, 7)
                          s1 in [0, 2)
                          s2 in [0, 6)
                        )"));
}

TEST_F(IndexingMapTest, Composition_ProducerAndConsumerHaveConstraints) {
  IndexingMap producer = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1, s0)", &mlir_context_),
      {50, 60}, {70, 20});
  producer.AddConstraint(ParseAffineExpr("d0 mod 8", &mlir_context_),
                         Interval{0, 0});
  producer.AddConstraint(ParseAffineExpr("s0 mod 3", &mlir_context_),
                         Interval{1, 1});

  IndexingMap consumer = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_), {10}, {8});
  consumer.AddConstraint(ParseAffineExpr("d0 + s0", &mlir_context_),
                         Interval{0, 20});
  consumer.AddConstraint(ParseAffineExpr("s0 mod 4", &mlir_context_),
                         Interval{0, 0});

  auto composed = ComposeIndexingMaps(consumer, producer);
  EXPECT_THAT(composed, MatchIndexingMap(R"(
                          (d0)[s0, s1, s2] -> (s2, d0, s1, s0)
                          domain:
                          d0 in [0, 10)
                          s0 in [0, 70)
                          s1 in [0, 20)
                          s2 in [0, 8)
                          d0 + s2 in [0, 21)
                          d0 mod 8 in [0, 1)
                          s0 mod 3 in [1, 2)
                          s2 mod 4 in [0, 1)
                        )"));
  EXPECT_TRUE(composed.Simplify());
  EXPECT_THAT(composed, MatchIndexingMap(R"(
                          (d0)[s0, s1, s2] -> (s2, d0, s1, s0)
                          domain:
                          d0 in [0, 9)
                          s0 in [1, 68)
                          s1 in [0, 20)
                          s2 in [0, 5)
                          d0 mod 8 in [0, 1)
                          s0 mod 3 in [1, 2)
                          s2 mod 4 in [0, 1)
                        )"));
}

TEST_F(IndexingMapTest, RemoveUnusedVars_ConstraintUsesDim) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (d1, s0, s1)", &mlir_context_),
      {50, 60}, {70, 20});
  // This constraint cannot be removed, because it contains a dimension.
  indexing_map.AddConstraint(ParseAffineExpr("s0 + d0", &mlir_context_),
                             Interval{1, 100});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 3", &mlir_context_),
                             Interval{0, 0});
  indexing_map.RemoveUnusedVars();
  EXPECT_THAT(indexing_map, MatchIndexingMap(R"(
                          (d0, d1)[s0, s1] -> (d1, s0, s1)
                          domain:
                          d0 in [0, 50)
                          d1 in [0, 60)
                          s0 in [0, 70)
                          s1 in [0, 20)
                          d0 + s0 in [1, 101)
                          s0 mod 3 in [0, 1)
                        )"));
}

TEST_F(IndexingMapTest, RemoveUnusedVars_ConstraintUsesUnusedDim) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (s0, d1, s1)", &mlir_context_),
      {50, 60}, {70, 20});
  // This constraint can be removed, because it contains only the unused dim.
  indexing_map.AddConstraint(ParseAffineExpr("d0 mod 3", &mlir_context_),
                             Interval{0, 0});
  indexing_map.RemoveUnusedVars();
  EXPECT_THAT(indexing_map, MatchIndexingMap(R"(
                          (d0)[s0, s1] -> (s0, d0, s1)
                          domain:
                          d0 in [0, 60)
                          s0 in [0, 70)
                          s1 in [0, 20)
                        )"));
}

TEST_F(IndexingMapTest, RemoveUnusedSymbols_ConstraintUsesOnlyUnusedSym) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (d0, d1, s1)", &mlir_context_),
      {50, 60}, {70, 20});
  // This constraint can be removed, because it contains only the unused symbol.
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 3", &mlir_context_),
                             Interval{0, 0});
  indexing_map.RemoveUnusedSymbols();
  EXPECT_THAT(indexing_map, MatchIndexingMap(R"(
                          (d0, d1)[s0] -> (d0, d1, s0)
                          domain:
                          d0 in [0, 50)
                          d1 in [0, 60)
                          s0 in [0, 20)
                        )"));
}

TEST_F(IndexingMapTest, RemoveUnusedVars_ConstraintsWithManyDims) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(
          "(d0, d1, d2, d3, d4)[s0, s1, s2] -> (s0 * 4 + d1 + d3 - 42)",
          &mlir_context_),
      {1, 2, 3, 4, 5}, {32, 64, 96});
  indexing_map.AddConstraint(
      ParseAffineExpr("s0 * 4 + d1 + d3", &mlir_context_), Interval{24, 459});
  indexing_map.AddConstraint(ParseAffineExpr("s0 + s2", &mlir_context_),
                             Interval{0, 512});
  auto unused_vars = indexing_map.RemoveUnusedVars();
  // dimensions d0, d2, d4 and symbol s1 will be removed.
  EXPECT_THAT(indexing_map, MatchIndexingMap(R"(
                              (d0, d1)[s0, s1] -> (d0 + s0 * 4 + d1 - 42)
                              domain:
                              d0 in [0, 2)
                              d1 in [0, 4)
                              s0 in [0, 32)
                              s1 in [0, 96)
                              d0 + s0 * 4 + d1 in [24, 460)
                              s0 + s1 in [0, 513)
                            )"));
  EXPECT_THAT(ConvertToSTL(unused_vars),
              ::testing::ElementsAreArray(
                  {true, false, true, false, true, false, true, false}));
}

TEST_F(IndexingMapTest, RemoveUnusedSymbols_ConstraintUsesSymbol) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1)", &mlir_context_),
      {50, 60}, {70, 20});
  // This constraint cannot be removed, because it contains a "used symbol".
  indexing_map.AddConstraint(ParseAffineExpr("s0 + s1", &mlir_context_),
                             Interval{1, 100});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 3", &mlir_context_),
                             Interval{0, 0});
  indexing_map.RemoveUnusedSymbols();
  EXPECT_THAT(indexing_map, MatchIndexingMap(R"(
                          (d0, d1)[s0, s1] -> (d1, d0, s1)
                          domain:
                          d0 in [0, 50)
                          d1 in [0, 60)
                          s0 in [0, 70)
                          s1 in [0, 20)
                          s0 + s1 in [1, 101)
                          s0 mod 3 in [0, 1)
                        )"));
}

TEST_F(IndexingMapTest, RemoveUnusedSymbols_ConstraintUsesOnlyUnusedSymbols) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1)", &mlir_context_),
      {50, 60}, {70, 20});
  // This constraint can be removed, because it contains only the unused symbol.
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 3", &mlir_context_),
                             Interval{0, 0});
  indexing_map.RemoveUnusedSymbols();
  EXPECT_THAT(indexing_map, MatchIndexingMap(R"(
                          (d0, d1)[s0] -> (d1, d0, s0)
                          domain:
                          d0 in [0, 50)
                          d1 in [0, 60)
                          s0 in [0, 20)
                        )"));
}

TEST_F(IndexingMapTest, RemoveUnusedSymbols_ConstraintIsAConstantWithinRange) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0) -> (d0)", &mlir_context_), {50}, {});
  indexing_map.AddConstraint(ParseAffineExpr("0", &mlir_context_),
                             Interval{-10, 5});
  EXPECT_THAT(indexing_map, MatchIndexingMap(R"(
                          (d0) -> (d0)
                          domain:
                          d0 in [0, 50)
                        )"));
}

TEST_F(IndexingMapTest, KnownEmpty_CreatingIndexingMapWithInfeasibleRange) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0) -> (d0)", &mlir_context_), {-1}, {});
  EXPECT_THAT(indexing_map, MatchIndexingMap("KNOWN EMPTY"));
}

TEST_F(IndexingMapTest, KnownEmpty_AddingConstraintOutOfRange) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0) -> (d0)", &mlir_context_), {50}, {});
  // Addition of this constraint makes the domain empty.
  indexing_map.AddConstraint(ParseAffineExpr("0", &mlir_context_),
                             Interval{10, 15});
  EXPECT_THAT(indexing_map, MatchIndexingMap("KNOWN EMPTY"));
}

TEST_F(IndexingMapTest, KnownEmpty_Composition) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0) -> (d0)", &mlir_context_), {50}, {});
  IndexingMap known_empty = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0) -> (0)", &mlir_context_), {0}, {});
  EXPECT_THAT(known_empty, MatchIndexingMap("KNOWN EMPTY"));
  EXPECT_THAT(indexing_map * known_empty, MatchIndexingMap("KNOWN EMPTY"));
  EXPECT_THAT(known_empty * indexing_map, MatchIndexingMap("KNOWN EMPTY"));
  EXPECT_EQ((indexing_map * known_empty).GetAffineMap().getNumResults(), 1);
  EXPECT_EQ((known_empty * indexing_map).GetAffineMap().getNumResults(), 1);
}

TEST_F(IndexingMapTest,
       KnownEmpty_AddingConstraintOutOfRangeAfterSimplification) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1)", &mlir_context_),
      {50, 60}, {70, 20});
  indexing_map.AddConstraint(ParseAffineExpr("s1 floordiv 20", &mlir_context_),
                             Interval{2, 2});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map, MatchIndexingMap("KNOWN EMPTY"));
}

TEST_F(IndexingMapTest, RemoveUnusedSymbols_ConstraintsWithManySymbols) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0, s1, s2, s3, s4] -> (d0 * 4 + s1 + s3 - 42)",
                     &mlir_context_),
      {32}, {1, 2, 3, 4, 5});
  indexing_map.AddConstraint(
      ParseAffineExpr("d0 * 4 + s1 + s3", &mlir_context_), Interval{24, 459});
  indexing_map.RemoveUnusedSymbols();
  // Symbols s0, s2, s4 will be removed and s1 and s3 will become s0 and s1.
  EXPECT_THAT(indexing_map, MatchIndexingMap(R"(
                              (d0)[s0, s1] -> (d0 * 4 + s0 + s1 - 42)
                              domain:
                              d0 in [0, 32)
                              s0 in [0, 2)
                              s1 in [0, 4)
                              d0 * 4 + s0 + s1 in [24, 460)
                            )"));
}

TEST_F(IndexingMapTest, RemoveUnusedSymbols_ConstraintsWithRTVars) {
  auto zero_dim_map = AffineMap::get(&mlir_context_);
  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0, s1, s2, s3, s4] -> (d0 * 4 + s1 + s3 - 42)",
                     &mlir_context_),
      {DimVar{{0, 31}}}, {RangeVar{{0, 0}}, RangeVar{{0, 1}}, RangeVar{{0, 2}}},
      {RTVar{Interval{0, 3},
             /*instr=*/nullptr, zero_dim_map},
       RTVar{Interval{0, 4},
             /*instr=*/nullptr, zero_dim_map}});
  indexing_map.AddConstraint(
      ParseAffineExpr("d0 * 4 + s1 + s3", &mlir_context_), Interval{24, 459});
  indexing_map.RemoveUnusedSymbols();
  // Symbols s0, s2, s4 will be removed and s1 and s3 will become s0 and s1.
  EXPECT_THAT(indexing_map, MatchIndexingMap(R"(
                              (d0)[s0, s1] -> (d0 * 4 + s0 + s1 - 42)
                              domain:
                              d0 in [0, 32)
                              s0 in [0, 2)
                              s1 in [0, 4)
                                hlo: NULL
                                () -> ()
                              d0 * 4 + s0 + s1 in [24, 460)
                            )"));
}

TEST_F(IndexingMapTest, ConstraintIntervalSimplification_Sum) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0) -> (d0)", &mlir_context_), {100}, {});

  indexing_map.AddConstraint(ParseAffineExpr("(d0 mod 8) + 5", &mlir_context_),
                             Interval{50, 54});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0) -> (d0)
                          domain:
                          d0 in [0, 100)
                          d0 mod 8 in [45, 50)
                        )"));
}

TEST_F(IndexingMapTest,
       ConstraintIntervalSimplification_Sum_IndependentOfSymbol) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0, s1] -> (d0 * 6 + s0 * 3 + s1)", &mlir_context_),
      {2000}, {2, 3});

  indexing_map.AddConstraint(
      ParseAffineExpr("d0 * 6 + s0 * 3 + s1", &mlir_context_),
      Interval{0, 599});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0)[s0, s1] -> (d0 * 6 + s0 * 3 + s1)
                          domain:
                          d0 in [0, 100)
                          s0 in [0, 2)
                          s1 in [0, 3)
                        )"));
}

TEST_F(IndexingMapTest,
       ConstraintIntervalSimplification_Sum_NotIndependentOfSymbol) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0, s1] -> (d0 * 6 + s0 * 3 + s1)", &mlir_context_),
      {2000}, {2, 3});

  indexing_map.AddConstraint(
      ParseAffineExpr("d0 * 6 + s0 * 3 + s1", &mlir_context_),
      Interval{0, 598});
  EXPECT_FALSE(indexing_map.Simplify());
}

TEST_F(IndexingMapTest, ConstraintIntervalSimplification_Sum_GcdGreaterOne) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0] -> (d0 * 6 + s0 * 3)", &mlir_context_), {2000},
      {2});

  indexing_map.AddConstraint(ParseAffineExpr("d0 * 6 + s0 * 3", &mlir_context_),
                             Interval{0, 599});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0)[s0] -> (d0 * 6 + s0 * 3)
                          domain:
                          d0 in [0, 100)
                          s0 in [0, 2)
                        )"));
}

TEST_F(IndexingMapTest,
       ConstraintIntervalSimplification_FloorDivPositiveDivisorPositiveBounds) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0) -> (d0)", &mlir_context_), {100}, {});

  indexing_map.AddConstraint(ParseAffineExpr("d0 floordiv 8", &mlir_context_),
                             Interval{5, 11});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0) -> (d0)
                          domain:
                          d0 in [40, 96)
                        )"));
}

TEST_F(IndexingMapTest,
       ConstraintIntervalSimplification_FloorDivPositiveDivisorNegativeBounds) {
  IndexingMap indexing_map =
      IndexingMap(ParseAffineMap("(d0)[s0] -> (d0)", &mlir_context_),
                  {DimVar{{0, 99}}}, {RangeVar{{-99, 99}}}, /*rt_vars=*/{});

  indexing_map.AddConstraint(ParseAffineExpr("s0 floordiv 3", &mlir_context_),
                             Interval{-11, -5});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0)[s0] -> (d0)
                          domain:
                          d0 in [0, 100)
                          s0 in [-33, -12)
                        )"));
}

TEST_F(IndexingMapTest,
       ConstraintIntervalSimplification_FloorDivNegativeDivisorNegativeBounds) {
  IndexingMap indexing_map =
      IndexingMap(ParseAffineMap("(d0)[s0] -> (d0)", &mlir_context_),
                  {DimVar{{0, 99}}}, {RangeVar{{-99, 99}}}, /*rt_vars=*/{});

  indexing_map.AddConstraint(ParseAffineExpr("s0 floordiv -3", &mlir_context_),
                             Interval{-11, -5});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0)[s0] -> (d0)
                          domain:
                          d0 in [0, 100)
                          s0 in [15, 36)
                        )"));
}

TEST_F(IndexingMapTest,
       ConstraintIntervalSimplification_MulPositiveMultiplierPositiveBounds) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0) -> (d0)", &mlir_context_), {100}, {});

  indexing_map.AddConstraint(ParseAffineExpr("d0 * 8", &mlir_context_),
                             Interval{14, 33});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0) -> (d0)
                          domain:
                          d0 in [2, 5)
                        )"));
}

TEST_F(IndexingMapTest,
       ConstraintIntervalSimplification_MulPositiveMultiplierNegativeBounds) {
  IndexingMap indexing_map =
      IndexingMap(ParseAffineMap("(d0)[s0] -> (d0)", &mlir_context_),
                  {DimVar{{0, 99}}}, {RangeVar{{-99, 99}}}, /*rt_vars=*/{});

  indexing_map.AddConstraint(ParseAffineExpr("s0 * 3", &mlir_context_),
                             Interval{-11, -5});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0)[s0] -> (d0)
                          domain:
                          d0 in [0, 100)
                          s0 in [-3, -1)
                        )"));
}

TEST_F(IndexingMapTest,
       ConstraintIntervalSimplification_MulNegativeMultiplierNegativeBounds) {
  IndexingMap indexing_map =
      IndexingMap(ParseAffineMap("(d0)[s0] -> (d0)", &mlir_context_),
                  {DimVar{{0, 99}}}, {RangeVar{{-99, 99}}}, /*rt_vars=*/{});

  indexing_map.AddConstraint(ParseAffineExpr("s0 * -3", &mlir_context_),
                             Interval{-11, -5});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0)[s0] -> (d0)
                          domain:
                          d0 in [0, 100)
                          s0 in [2, 4)
                        )"));
}

TEST_F(IndexingMapTest, ConstraintMerge_Mod) {
  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0, s1] -> (d0, s1, s0)", &mlir_context_),
      {DimVar{{0, 4}}}, {RangeVar{{-21, -1}}, RangeVar{{0, 10}}},
      /*rt_vars=*/{});
  indexing_map.AddConstraint(ParseAffineExpr("d0 mod 3", &mlir_context_),
                             Interval{0, 0});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 2", &mlir_context_),
                             Interval{0, 0});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 3", &mlir_context_),
                             Interval{0, 0});
  indexing_map.AddConstraint(ParseAffineExpr("s1 mod 5", &mlir_context_),
                             Interval{1, 1});
  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(), MatchIndexingString(R"(
                          (d0)[s0, s1] -> (d0, s1, s0)
                          domain:
                          d0 in [0, 4)
                          s0 in [-18, -5)
                          s1 in [1, 7)
                          d0 mod 3 in [0, 1)
                          s0 mod 6 in [0, 1)
                          s1 mod 5 in [1, 2)
                        )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_ConstantDims) {
  IndexingMap indexing_map =
      IndexingMap(ParseAffineMap("(d0) -> (d0)", &mlir_context_),
                  {DimVar{{5, 5}}}, /*range_vars=*/{}, /*rt_vars=*/{});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                                                  (d0) -> (5)
                                                  domain:
                                                  d0 in [5, 6)
                                                )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_SumOrderRegression) {
  // This is a regression test for a bug where we didn't canonicalize the order
  // of summands correctly, leading to `Simplify` not being idempotent.
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0, d1)[s0, s1] -> (((((d0 + (d0 mod 3)) floordiv 3) + "
                     "(s0 + ((s0 + s0) mod 3))) + (((d0 + s0) mod 3) + 0)))",
                     &mlir_context_),
      {10, 20}, {30, 40});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_FALSE(indexing_map.Simplify());
}

TEST_F(IndexingMapTest, AffineMapSimplification_SumOrderRegression2) {
  // This is a regression test for a bug where we didn't simplify the affine
  // expression fully after a single iteration.
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0] -> ((((s0 + d0) + d0) floordiv 2))",
                     &mlir_context_),
      {10, 20}, {30, 40});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_FALSE(indexing_map.Simplify());
}

TEST_F(IndexingMapTest, AffineMapSimplification_ModIsSub) {
  IndexingMap indexing_map(
      ParseAffineMap("(d0) -> (d0 mod 42)", &mlir_context_), {{53, 71}}, {},
      {});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                                                 (d0) -> (d0 - 42)
                                                 domain:
                                                 d0 in [53, 72)
                                               )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_ModIsAdd) {
  IndexingMap indexing_map(ParseAffineMap("(d0) -> (d0 mod 5)", &mlir_context_),
                           {{-5, -1}}, {}, {});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                                                 (d0) -> (d0 + 5)
                                                 domain:
                                                 d0 in [-5, 0)
                                               )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_ModIsNotAdd) {
  IndexingMap indexing_map1(
      ParseAffineMap("(d0) -> (d0 mod 5)", &mlir_context_), {{-4, 0}}, {}, {});
  EXPECT_FALSE(indexing_map1.Simplify());
  IndexingMap indexing_map2(
      ParseAffineMap("(d0) -> (d0 mod 5)", &mlir_context_), {{-6, -1}}, {}, {});
  EXPECT_FALSE(indexing_map2.Simplify());
}

TEST_F(IndexingMapTest, AffineMapSimplification_SubIsMod) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0] -> (d0 - (s0 floordiv 3) * 3 + s0)",
                     &mlir_context_),
      {2}, {4});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                                                 (d0)[s0] -> (d0 + s0 mod 3)
                                                 domain:
                                                 d0 in [0, 2)
                                                 s0 in [0, 4)
                                               )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_SubIsModMultiplied) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0] -> (d0 - (s0 floordiv 3) * 12 + s0 * 7)",
                     &mlir_context_),
      {2}, {4});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                (d0)[s0] -> (d0 + (s0 mod 3) * 4 + s0 * 3)
                domain:
                d0 in [0, 2)
                s0 in [0, 4)
              )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_SubIsModSum) {
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap("(d0)[s0] ->  (1 + d0 - ((s0 + 1) floordiv 3) * 3 + s0)",
                     &mlir_context_),
      {2}, {4});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                (d0)[s0] -> (d0 + (s0 + 1) mod 3)
                domain:
                d0 in [0, 2)
                s0 in [0, 4)
              )"));
}

TEST_F(IndexingMapTest,
       AffineMapSimplification_DivsAndModsIfSmallerThanDivisor) {
  auto serialized_map = "(d0, d1) -> (d0 + d1 floordiv 16, d1 mod 16)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {8, 16}, {});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                                                  (d0, d1) -> (d0, d1)
                                                  domain:
                                                  d0 in [0, 8)
                                                  d1 in [0, 16)
                                                )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_DivsAndModsWithMultipliers) {
  auto serialized_map =
      "(d0, d1, d2) -> ((d0 * 100 + d1 * 10 + d2) floordiv 100, "
      "((d0 * 100 + d1 * 10 + d2) mod 100) floordiv 10, "
      "d2 mod 10)";

  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {9, 9, 9}, {});
  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                                                  (d0, d1, d2) -> (d0, d1, d2)
                                                  domain:
                                                  d0 in [0, 9)
                                                  d1 in [0, 9)
                                                  d2 in [0, 9)
                                                )"));
}

TEST_F(IndexingMapTest,
       AffineMapSimplification_DivsAndModsWithDivisibleMultipliers) {
  auto serialized_map =
      "(d0, d1, d2) -> ((d0 * 16 + d1 * 4 + d2) floordiv 8, "
      "                 (d0 * 16 + d1 * 4 + d2) mod 8)";

  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {10, 10, 10}, {});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
    (d0, d1, d2) -> (d0 * 2 + (d1 * 4 + d2) floordiv 8,
                     (d1 * 4 + d2) mod 8)
    domain:
    d0 in [0, 10)
    d1 in [0, 10)
    d2 in [0, 10)
  )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_DivsAndModsWithReverse) {
  auto serialized_map =
      "(d0, d1) -> (-((d0 * -11 - d1 + 109) floordiv 11) + 9, "
      "d0 * 11 + d1 + ((d0 * -11 - d1 + 109) floordiv 11) * 11 - 99)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {8, 9}, {});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                                                 (d0, d1) -> (d0, d1)
                                                 domain:
                                                 d0 in [0, 8)
                                                 d1 in [0, 9)
                                               )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_SimplifyReshape) {
  auto serialized_map =
      "()[s0] -> ((s0 * 128) mod 715 + ((s0 * 128) floordiv 715) * 715)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {}, {128});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      ()[s0] -> (s0 * 128)
      domain: s0 in [0, 128)
  )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_SimplifyReshape2) {
  auto serialized_map =
      "(d0, d1) -> ((d0 mod 8) * 128 + d1 + (d0 floordiv 8) * 1024)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {1024, 128}, {});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      (d0, d1) -> (d0 * 128 + d1)
      domain:
      d0 in [0, 1024)
      d1 in [0, 128)
  )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_SimplifyReshape3) {
  auto serialized_map =
      "(d0, d1) -> (((d1 * 2 + d0 floordiv 64) mod 3) * 256 + (d0 mod 64) * 4 "
      "+ ((d1 * 128 + d0) floordiv 192) * 768)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {128, 3072}, {});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      (d0, d1) -> (d0 * 4 + d1 * 512)
      domain:
      d0 in [0, 128)
      d1 in [0, 3072)
  )"));
}

TEST_F(IndexingMapTest,
       AffineMapSimplification_ModWithNegativeMultiplerDoesNotGetSimplified) {
  auto serialized_map = "(d0) -> ((-d0) mod 2)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {128}, {});
  EXPECT_FALSE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      (d0) -> ((-d0) mod 2)
      domain:
      d0 in [0, 128)
  )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_SimplifyBitcastAndBack) {
  // `d0 floordiv 1536` is the result of simplifying this:
  // `((d0 * 2 + d1 floordiv 64) floordiv 3) floordiv 1024`.
  // This test verifies that we can still simplify the map after the
  // simplification of the floordiv.
  auto serialized_map =
      "(d0, d1) -> ((d0 floordiv 1536) * 786432 + (((d0 * 2 + d1 floordiv "
      "64) floordiv 3) mod 1024) * 768 + ((d0 * 2 + d1 floordiv 64) mod 3) * "
      "256 + (d1 mod 64) * 4)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {3072, 128}, {});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      (d0, d1) -> (d0 * 512 + d1 * 4)
      domain:
      d0 in [0, 3072)
      d1 in [0, 128)
  )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_SimplifyReshape_Regression) {
  // We have s0 * 128 in the mod, but s0 * 64 in the floordiv *.
  auto serialized_map =
      "()[s0] -> ((s0 * 128) mod 715 + ((s0 * 64) floordiv 715) * 715)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {}, {128});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      ()[s0] -> (((s0 * 64) floordiv 715) * 715 + (s0 * 128) mod 715)
      domain: s0 in [0, 128)
  )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_DivsInSequence) {
  auto serialized_map =
      "()[s0] -> (s0 - ((s0 floordiv 2) floordiv 7) * 14 + (s0 floordiv 14) * "
      "14)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {}, {1234});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
                                                 ()[s0] -> (s0)
                                                 domain:
                                                 s0 in [0, 1234)
                                               )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_DivDiv) {
  auto serialized_map = "()[s0, s1] -> ((s0 * 2 + s1 floordiv 64) floordiv 3)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {}, {1234, 128});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      ()[s0, s1] -> ((s0 * 128 + s1) floordiv 192)
      domain:
      s0 in [0, 1234)
      s1 in [0, 128)
    )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_DivSumConstant) {
  auto serialized_map = "()[s0] -> ((s0 * 6 + 9) floordiv 18)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {}, {1234});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      ()[s0] -> ((s0 * 2 + 3) floordiv 6)
      domain:
      s0 in [0, 1234)
    )"));
}

TEST_F(IndexingMapTest, AffineMapSimplification_DivSumDiv) {
  auto serialized_map =
      "()[s0, s1] -> ((s0 floordiv 3 + s1 floordiv 3) floordiv 6)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {}, {1234, 128});
  // The rewrite tested in AffineMapSimplification_DivDiv must not trigger here.
  EXPECT_FALSE(indexing_map.Simplify());
}

TEST_F(IndexingMapTest, AffineMapSimplification_NegativeDiv) {
  // (s0 floordiv 2) floordiv -7 is not s0 floordiv -14:
  // 15 // 2 // -7 = -1
  // 15 // -14 = -2
  auto serialized_map = "()[s0] -> ((s0 floordiv 2) floordiv -7)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {}, {1234});
  EXPECT_FALSE(indexing_map.Simplify());
}

TEST_F(IndexingMapTest, AffineMapSimplification_ExtractFromMod) {
  auto serialized_map =
      "()[s0, s1, s2, s3] -> ((s0 * 458752 + s1 + s2 * 4 + s3 * 512) mod "
      "20000)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {}, {872, 4, 128, 896});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      ()[s0, s1, s2, s3] -> (
        ((s0 * 114688 + s3 * 128 + s2) mod 5000) * 4 + s1
      )
      domain:
      s0 in [0, 872)
      s1 in [0, 4)
      s2 in [0, 128)
      s3 in [0, 896)
    )"));
}

TEST_F(IndexingMapTest,
       AffineMapSimplification_ExtractFromDiv_NegativeMultiplier) {
  auto serialized_map =
      "()[s0, s1] -> ((s0 * 16 - (s1 floordiv 4) floordiv 2 + (s1 floordiv 8) "
      "* 2) floordiv 4)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {}, {2, 128});
  EXPECT_TRUE(indexing_map.Simplify());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      ()[s0, s1] -> (
        s0 * 4 + s1 floordiv 32
      )
      domain:
      s0 in [0, 2)
      s1 in [0, 128)
    )"));
}

TEST_F(IndexingMapTest, RescaleSymbols_Simple) {
  auto serialized_map = "(d0)[s0, s1, s2] -> (s2, d0, s1, s0 floordiv 6)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {4}, {7, 2, 6});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 6", &mlir_context_),
                             Interval{0, 0});

  EXPECT_TRUE(indexing_map.RescaleSymbols());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      (d0)[s0, s1, s2] -> (s2, d0, s1, s0)
      domain:
        d0 in [0, 4)
        s0 in [0, 2)
        s1 in [0, 2)
        s2 in [0, 6)
    )"));
}

TEST_F(IndexingMapTest, RescaleSymbols_WithShift) {
  auto serialized_map = "(d0)[s0, s1, s2] -> (s2, d0, s1, s0)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {4}, {42, 2, 6});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 6", &mlir_context_),
                             Interval{3, 3});

  // [BEFORE] Allowed values for s0: 3, 9, 15, ..., 39 = (6 * 6 + 3)
  // [AFTER] Allowed values for s0: 0, 1, 2, ..., 6
  EXPECT_TRUE(indexing_map.RescaleSymbols());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      (d0)[s0, s1, s2] -> (s2, d0, s1, s0 * 6 + 3)
      domain:
        d0 in [0, 4)
        s0 in [0, 7)
        s1 in [0, 2)
        s2 in [0, 6)
    )"));
}

TEST_F(IndexingMapTest, RescaleSymbols_TwoModConstraints) {
  auto serialized_map = "(d0)[s0, s1, s2] -> (s2, d0, s1, s0 floordiv 6)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {4}, {7, 2, 6});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 2", &mlir_context_),
                             Interval{0, 0});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 3", &mlir_context_),
                             Interval{0, 0});

  EXPECT_TRUE(indexing_map.RescaleSymbols());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      (d0)[s0, s1, s2] -> (s2, d0, s1, s0)
      domain:
        d0 in [0, 4)
        s0 in [0, 2)
        s1 in [0, 2)
        s2 in [0, 6)
    )"));
}

TEST_F(IndexingMapTest, RescaleSymbols_RescaledSymbolInOtherNonModConstraint) {
  auto serialized_map = "(d0)[s0, s1, s2] -> (s2, d0, s1, s0)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {4}, {10, 2, 6});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 6", &mlir_context_),
                             Interval{3, 3});
  indexing_map.AddConstraint(ParseAffineExpr("s0 * s2", &mlir_context_),
                             Interval{0, 28});

  EXPECT_TRUE(indexing_map.RescaleSymbols());
  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
      (d0)[s0, s1, s2] -> (s2, d0, s1, s0 * 6 + 3)
      domain:
        d0 in [0, 4)
        s0 in [0, 2)
        s1 in [0, 2)
        s2 in [0, 6)
        (s0 * 6 + 3) * s2 in [0, 29)
    )"));
}

TEST_F(IndexingMapTest,
       RescaleSymbols_TwoModConstraintsForTheSameSymbolWhichCannotBeMerged) {
  auto serialized_map = "(d0)[s0, s1, s2] -> (s2, d0, s1, s0)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {4}, {100, 2, 6});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 6", &mlir_context_),
                             Interval{3, 3});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 7", &mlir_context_),
                             Interval{5, 5});

  EXPECT_TRUE(indexing_map.RescaleSymbols());

  const mlir::AffineExpr result3 = indexing_map.GetAffineMap().getResult(3);
  ASSERT_THAT(indexing_map.GetConstraints(), ::testing::SizeIs(1));
  const mlir::AffineExpr constraint_expr =
      indexing_map.GetConstraints().begin()->first;
  const Interval constraint_interval =
      indexing_map.GetConstraints().begin()->second;

  // TODO(b/347240603): This case is not yet fully supported, because the
  // resulting indexing map depends on the hashmap iteration order, so it can
  // have different values randomly. Also the range of s0 can depend on the
  // iteration order and how many times we simplify. Maybe this case is not so
  // important for now.
  EXPECT_THAT(
      std::make_tuple(result3, constraint_expr, constraint_interval),
      AnyOf(
          std::make_tuple(ParseAffineExpr("s0 * 6 + 3", &mlir_context_),
                          ParseAffineExpr("(s0 * 6 + 3) mod 7", &mlir_context_),
                          Interval{5, 5}),
          std::make_tuple(ParseAffineExpr("s0 * 7 + 5", &mlir_context_),
                          ParseAffineExpr("(s0 * 7 + 5) mod 6", &mlir_context_),
                          Interval{3, 3})));
}

TEST_F(IndexingMapTest, RescaleSymbolsKeepsHashmapConsistent) {
  auto serialized_map = "(d0)[s0, s1, s2] -> (s2, d0, s0, s0 floordiv 6)";
  IndexingMap indexing_map = IndexingMap::FromTensorSizes(
      ParseAffineMap(serialized_map, &mlir_context_), {4}, {7, 2, 6});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 6", &mlir_context_),
                             Interval{0, 0});
  indexing_map.AddConstraint(ParseAffineExpr("s0 * s1", &mlir_context_),
                             Interval{0, 100});

  EXPECT_TRUE(indexing_map.RescaleSymbols());

  for (auto& [expr, interval] : indexing_map.GetConstraints()) {
    EXPECT_TRUE(indexing_map.GetConstraints().contains(expr))
        << "Don't modify the *keys* of the hashmap.";
  }
}

TEST_F(IndexingMapTest, RangeEvaluatorTest) {
  auto serialized_map = "(d0, d1, d2, d3)[] -> (0)";
  IndexingMap indexing_map(ParseAffineMap(serialized_map, &mlir_context_),
                           {{Interval{0, 9}},
                            {Interval{-10, -1}},
                            {Interval{-1, 2}},
                            {Interval{0, 0}}},
                           {}, {});
  RangeEvaluator range_evaluator(indexing_map, &mlir_context_);
  mlir::AffineExpr d0, d1, d2, d3;
  bindDims(&mlir_context_, d0, d1, d2, d3);

  // d0 is always positive.
  EXPECT_TRUE(range_evaluator.IsAlwaysPositiveOrZero(d0));
  EXPECT_FALSE(range_evaluator.IsAlwaysNegativeOrZero(d0));

  // d1 is always negative.
  EXPECT_FALSE(range_evaluator.IsAlwaysPositiveOrZero(d1));
  EXPECT_TRUE(range_evaluator.IsAlwaysNegativeOrZero(d1));

  // d2 is sometimes positive and sometimes negative.
  EXPECT_FALSE(range_evaluator.IsAlwaysPositiveOrZero(d2));
  EXPECT_FALSE(range_evaluator.IsAlwaysNegativeOrZero(d2));

  // d3 is always 0.
  EXPECT_TRUE(range_evaluator.IsAlwaysPositiveOrZero(d3));
  EXPECT_TRUE(range_evaluator.IsAlwaysNegativeOrZero(d3));
}

TEST(IntervalComparisonTest, PointComparisons) {
  Interval interval{12, 64};
  auto point = [](int64_t n) { return Interval{n, n}; };
  EXPECT_EQ(interval.Gt(point(11)), true);
  EXPECT_EQ(interval.Gt(point(12)), std::nullopt);
  EXPECT_EQ(interval.Gt(point(65)), false);

  EXPECT_EQ(interval.Lt(point(65)), true);
  EXPECT_EQ(interval.Lt(point(64)), std::nullopt);
  EXPECT_EQ(interval.Lt(point(10)), false);

  EXPECT_EQ(interval.Eq(point(11)), false);
  EXPECT_EQ(interval.Eq(point(12)), std::nullopt);
  EXPECT_EQ(interval.Eq(point(15)), std::nullopt);
  EXPECT_EQ(interval.Eq(point(65)), false);

  EXPECT_EQ(interval.Ne(point(11)), true);
  EXPECT_EQ(interval.Ne(point(15)), std::nullopt);
  EXPECT_EQ(interval.Ne(point(65)), true);

  EXPECT_EQ(interval.Ge(point(12)), true);
  EXPECT_EQ(interval.Ge(point(64)), std::nullopt);
  EXPECT_EQ(interval.Ge(point(65)), false);

  EXPECT_EQ(interval.Le(point(11)), false);
  EXPECT_EQ(interval.Le(point(64)), true);
  EXPECT_EQ(interval.Le(point(63)), std::nullopt);
  EXPECT_EQ(interval.Le(point(65)), true);

  EXPECT_EQ(point(15).Eq(point(15)), true);
  EXPECT_EQ(point(15).Eq(point(16)), false);

  EXPECT_EQ(point(15).Ne(point(15)), false);
  EXPECT_EQ(point(15).Ne(point(16)), true);
}

TEST(IntervalComparisonTest, RangeComparisons) {
  Interval interval{12, 64};
  auto range = [](int64_t l, int64_t u) { return Interval{l, u}; };
  EXPECT_EQ(interval.Gt(range(-10, 11)), true);
  EXPECT_EQ(interval.Gt(range(-10, 12)), std::nullopt);
  EXPECT_EQ(interval.Gt(interval), std::nullopt);
  EXPECT_EQ(interval.Gt(range(10, 20)), std::nullopt);
  EXPECT_EQ(interval.Gt(range(50, 60)), std::nullopt);
  EXPECT_EQ(interval.Gt(range(64, 100)), false);
  EXPECT_EQ(interval.Gt(range(65, 100)), false);

  EXPECT_EQ(interval.Lt(range(65, 100)), true);
  EXPECT_EQ(interval.Lt(range(64, 100)), std::nullopt);
  EXPECT_EQ(interval.Lt(interval), std::nullopt);
  EXPECT_EQ(interval.Lt(range(50, 60)), std::nullopt);
  EXPECT_EQ(interval.Lt(range(10, 20)), std::nullopt);
  EXPECT_EQ(interval.Lt(range(-10, 12)), false);
  EXPECT_EQ(interval.Lt(range(-10, 11)), false);

  EXPECT_EQ(interval.Eq(interval), std::nullopt);
  EXPECT_EQ(interval.Eq(range(65, 100)), false);
  EXPECT_EQ(interval.Eq(range(0, 11)), false);
}

MATCHER_P(IntervalIs, interval, "") {
  std::pair<int64_t, int64_t> arg_pair{arg.lower, arg.upper};
  return ::testing::ExplainMatchResult(
      ::testing::Pair(interval.lower, interval.upper), arg_pair,
      result_listener);
}

TEST(IntervalMathTest, Addition) {
  Interval a{12, 64};
  Interval b{-100, 120};
  Interval sum{12 - 100, 64 + 120};
  EXPECT_THAT(a + b, IntervalIs(sum));
}

TEST(IntervalMathTest, AdditionSaturating) {
  Interval a{12, 64};
  Interval b{-100, 120};
  Interval c{100, std::numeric_limits<int64_t>::max() - 80};
  Interval any{std::numeric_limits<int64_t>::min(),
               std::numeric_limits<int64_t>::max()};
  Interval positive{0, std::numeric_limits<int64_t>::max()};
  Interval negative{std::numeric_limits<int64_t>::min(), 0};
  auto range = [](int64_t l, int64_t u) { return Interval{l, u}; };

  EXPECT_THAT(positive + negative, IntervalIs(any));
  EXPECT_THAT(any + any, IntervalIs(any));
  EXPECT_THAT(b + any, IntervalIs(any));

  EXPECT_THAT(c + any, IntervalIs(any));
  EXPECT_THAT(c + positive,
              IntervalIs(range(100, std::numeric_limits<int64_t>::max())));
  Interval c_plus_negative{negative.lower, c.upper};
  EXPECT_THAT(c + negative, IntervalIs(c_plus_negative));

  Interval a_plus_c{112, std::numeric_limits<int64_t>::max() - 16};
  EXPECT_THAT(a + c, IntervalIs(a_plus_c));
  Interval b_plus_c{0, std::numeric_limits<int64_t>::max()};
  EXPECT_THAT(b + c, IntervalIs(b_plus_c));
}

TEST(IntervalMathTest, Multiplication) {
  Interval pos{10, 100};
  Interval neg{-10, -1};
  Interval both_small{-5, 6};
  Interval both_large{-20, 1000};

  auto range = [](int64_t l, int64_t u) { return Interval{l, u}; };
  EXPECT_THAT(pos * neg, IntervalIs(range(-1000, -10)));
  EXPECT_THAT(pos * both_small, IntervalIs(range(-500, 600)));
  EXPECT_THAT(pos * both_large, IntervalIs(range(-2000, 100000)));
  EXPECT_THAT(neg * both_small, IntervalIs(range(-60, 50)));
  EXPECT_THAT(neg * both_large, IntervalIs(range(-10000, 200)));
  EXPECT_THAT(both_small * both_large, IntervalIs(range(-5000, 6000)));
}

TEST(IntervalMathTest, MultiplicationSaturating) {
  Interval any{std::numeric_limits<int64_t>::min(),
               std::numeric_limits<int64_t>::max()};
  Interval bit33{42, std::numeric_limits<uint32_t>::max()};
  Interval bit33_sq{42 * 42, std::numeric_limits<int64_t>::max()};
  EXPECT_THAT(bit33 * bit33, IntervalIs(bit33_sq));
  EXPECT_THAT(any * any, IntervalIs(any));

  Interval greater_41{42, std::numeric_limits<int64_t>::max()};
  Interval neg_one{-1, -1};
  Interval less_neg_41{std::numeric_limits<int64_t>::min(), -42};
  EXPECT_THAT(greater_41 * neg_one, IntervalIs(less_neg_41));
  EXPECT_THAT(less_neg_41 * neg_one, IntervalIs(greater_41));
  EXPECT_THAT(any * neg_one, IntervalIs(any));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_ScalarConstant) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        ROOT %constant = s64[] constant(42)
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  IndexingMap indexing_map(
      ParseAffineMap("()[s0] -> (s0)", &mlir_context_),
      /*dimensions=*/{},
      /*range_vars=*/{},
      {RTVar{Interval{42, 42},
             hlo_module.value()->entry_computation()->root_instruction(),
             AffineMap::get(0, 0, {}, &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              () -> (42)
              domain:
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_StaticIndexIntoTensorConstant) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        ROOT %constant = s64[2, 4]{1,0} constant({{1, 2, 3, 4}, {11, 12, 13, 14}})
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  IndexingMap indexing_map(
      ParseAffineMap("()[s0] -> (s0)", &mlir_context_),
      /*dimensions=*/{},
      /*range_vars=*/{},
      {RTVar{Interval{1, 14},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("() -> (1,2)", &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              () -> (13)
              domain:
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_NonFoldableTensor) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        ROOT %constant = s64[2, 4]{1,0} constant({{1, 2, 3, 4}, {11, 12, 13, 14}})
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (s0)", &mlir_context_),
      /*dimensions=*/{},
      /*range_vars=*/{},
      {RTVar{Interval{1, 14},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("(d0) -> (1, d0)", &mlir_context_)}});

  EXPECT_FALSE(indexing_map.Simplify());
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_Iota) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        ROOT %iota = s64[10, 10]{1,0} iota(), iota_dimension=0
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_),
      /*dimensions=*/{{0, 255}},
      /*range_vars=*/{},
      {RTVar{Interval{0, 9},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("(d0) -> (d0, 7)", &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0) -> (d0, d0)
              domain:
              d0 in [0, 256)
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_IotaAsConstant) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        ROOT %iota = s64[10, 10]{1,0} iota(), iota_dimension=1
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_),
      /*dimensions=*/{{0, 255}},
      /*range_vars=*/{},
      {RTVar{Interval{0, 9},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("(d0) -> (d0, 7)", &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0) -> (d0, 7)
              domain:
              d0 in [0, 256)
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_ConstraintsGetUpdated) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        ROOT %iota = s64[10, 10]{1,0} iota(), iota_dimension=0
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_),
      /*dimensions=*/{{0, 255}},
      /*range_vars=*/{},
      {RTVar{Interval{0, 9},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("(d0) -> (d0, 7)", &mlir_context_)}});
  indexing_map.AddConstraint(ParseAffineExpr("s0 mod 2", &mlir_context_),
                             Interval{0, 0});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0) -> (d0, d0)
              domain:
              d0 in [0, 255)
              d0 mod 2 in [0, 1)
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_Broadcast) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        %iota = s64[12]{0} iota(), iota_dimension=0
        ROOT %broadcast = s64[32, 12]{1,0} broadcast(s64[12]{0} %iota), dimensions={1}
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  // (d0, 11): d0 maps into the broadcasted dimension, so it doesn't matter
  // and 11 maps to 11 in iota.
  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_),
      /*dimensions=*/{{0, 31}},
      /*range_vars=*/{},
      {RTVar{Interval{0, 11},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("(d0) -> (d0, 11)", &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0) -> (d0, 11)
              domain:
              d0 in [0, 32)
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_ChainedNoncomputeOps) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        %iota = s64[12]{0} iota(), iota_dimension=0
        %reverse = s64[12]{0} reverse(s64[12]{0} %iota), dimensions={0}
        %reshape = s64[3,4]{1,0} reshape(s64[12]{0} %reverse)
        ROOT %broadcast = s64[36,3,4]{2,1,0} broadcast(s64[3,4]{1,0} %reshape), dimensions={1,2}
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  // - Iota: [0, 1, ,,,, 11]
  // - Reverse: [11, 10, ..., 0]
  // - Reshape: [[11, 10, 9, 8], [7, 6, 5, 4], [3, 2, 1, 0]]
  // - Coordinates: (d0 floordiv 12, 3)
  // - y-coordinate=3 means we index into [8, 4, 0]
  // - x-coordinate=(d0 floordiv 12) means our constant looks like this:
  //   [8, ..., 8, 4, ..., 4, 0, ..., 0]
  // - Hence our final expression: (d0 floordiv 12) * -4 + 8
  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_),
      /*dimensions=*/{{0, 35}},
      /*range_vars=*/{},
      {RTVar{
          Interval{0, 11},
          hlo_module.value()->entry_computation()->root_instruction(),
          ParseAffineMap("(d0) -> (d0, d0 floordiv 12, 3)", &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0) -> (d0, (d0 floordiv 12) * -4 + 8)
              domain:
              d0 in [0, 36)
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_PartialRTVarRemoval) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        %constant = s64[12]{0} constant({...})
        ROOT %broadcast = s64[24,12]{1,0} broadcast(s64[12]{0} %constant), dimensions={1}
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  // (d0, d0 floordiv 2): d0 maps into the broadcasted dimension, so it can't be
  // removed, but d0 floordiv 2 doesn't yield an affine expression so we need to
  // keep the RTVar, but can optimize it by removing the broadcast.
  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_),
      /*dimensions=*/{{0, 23}},
      /*range_vars=*/{},
      {RTVar{Interval{0, 512},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("(d0) -> (d0, d0 floordiv 2)", &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0)[s0] -> (d0, s0)
              domain:
              d0 in [0, 24)
              s0 in [0, 513)
                hlo: %constant = s64[12]{0} constant({...})
                (d0) -> (d0 floordiv 2)
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_Add) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        %constant = s64[] constant(42)
        %broadcast = s64[12,13,24]{2,1,0} broadcast(s64[] %constant), dimensions={}
        %iota = s64[12,13,24]{2,1,0} iota(), iota_dimension=2
        ROOT %add = s64[12,13,24]{2,1,0} add(s64[12,13,24]{2,1,0} %broadcast, s64[12,13,24]{2,1,0} %iota)
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  // The iota dimension is the last dimension in (d0, 7, 2 * d0), hence this
  // composes to 42 + 2 * d0
  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_),
      /*dimensions=*/{{0, 11}},
      /*range_vars=*/{},
      {RTVar{Interval{0, 11},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("(d0) -> (d0, 7, 2 * d0)", &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0) -> (d0, d0 * 2 + 42)
              domain:
              d0 in [0, 12)
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_Multiply) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        %iota0 = s64[12,12]{1,0} iota(), iota_dimension=0
        %iota1 = s64[12]{0} iota(), iota_dimension=0
        %broadcast = s64[12,12]{1,0} broadcast(s64[12]{0} %iota1), dimensions={1}
        %multiply = s64[12,12]{1,0} multiply(s64[12,12]{1,0} %iota0, s64[12,12]{1,0} %broadcast)
        ROOT %reverse = s64[12,12]{1,0} reverse(s64[12,12]{1,0} %multiply), dimensions={0}
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  // Iota0: [[0, ..., 0], [1, ..., 1], ..., [11, ..., 11]]
  // Iota1: [0, ..., 11]
  // Broadcast1: [[0, 1, ..., 11], [0, 1, ..., 11], ..., [0, 1, ..., 11]]
  // Mul: [[0, .., 0], [0, 1, ..., 11], [0, 2, ..., 22], ..., [0, 11, ..., 121]]
  // Reverse: [[0, 11, ..., 121], [0, 10, ..., 110], ..., [0, ..., 0]]
  // Therefore (d0, d0) evaluates to: (11 - d0) * d0.
  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_),
      /*dimensions=*/{{0, 11}},
      /*range_vars=*/{},
      {RTVar{Interval{0, 11},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("(d0) -> (d0, d0)", &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0) -> (d0, (-d0 + 11) * d0)
              domain:
              d0 in [0, 12)
              )"));
}

TEST_F(IndexingMapTest, ReplaceConstantRTVars_PartiallyOptimizableAdd) {
  absl::StatusOr<std::unique_ptr<VerifiedHloModule>> hlo_module =
      ParseAndReturnVerifiedModule(R"hlo(
      HloModule m

      ENTRY e {
        %constant = s64[12]{0} constant({...})
        %broadcast = s64[12,13,24]{2,1,0} broadcast(s64[12]{0} %constant), dimensions={0}
        %iota = s64[12,13,24]{2,1,0} iota(), iota_dimension=2
        ROOT %add = s64[12,13,24]{2,1,0} add(s64[12,13,24]{2,1,0} %broadcast, s64[12,13,24]{2,1,0} %iota)
      }
    )hlo");

  ASSERT_TRUE(hlo_module.ok());

  // The iota dimension is the last dimension in (d0, 7, 2 * d0), the constant
  // only depends on the first dimension. The constant consists of some
  // arbitrary values that cannot be represent as an affine expression, hence
  // the RTVar remains in-place.
  IndexingMap indexing_map(
      ParseAffineMap("(d0)[s0] -> (d0, s0)", &mlir_context_),
      /*dimensions=*/{{0, 11}},
      /*range_vars=*/{},
      {RTVar{Interval{0, 11},
             hlo_module.value()->entry_computation()->root_instruction(),
             ParseAffineMap("(d0) -> (d0, 7, 2 * d0)", &mlir_context_)}});

  EXPECT_TRUE(indexing_map.Simplify());

  EXPECT_THAT(indexing_map.ToString(printer_), MatchIndexingString(R"(
              (d0)[s0] -> (d0, d0 * 2 + s0)
              domain:
              d0 in [0, 12)
              s0 in [0, 12)
                hlo: %constant = s64[12]{0} constant({...})
                (d0) -> (d0)
              )"));
}

template <typename T>
void ExpectSupportsAbslHashAndEqAndNe(absl::Span<const T> values) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(values));

  // C++20 compilers automatically generate != from ==, but XLA has to work with
  // C++17, so we test that we explicitly implemented !=. Otherwise it could
  // happen that some compilers can compile XLA, and some can't.
  for (const T& a : values) {
    for (const T& b : values) {
      EXPECT_EQ(a != b, !(a == b));
    }
  }
}

TEST_F(IndexingMapTest, IntervalSupportsAbslHashAndEqAndNe) {
  ExpectSupportsAbslHashAndEqAndNe<Interval>(
      {Interval{1, 1}, Interval{0, 1}, Interval{1, 2}});
}

TEST_F(IndexingMapTest, IntervalSupportsLlvmStyleHashingAndEqAndNe) {
  auto check_consistent = [](const Interval& a, const Interval& b) {
    if (a == b) {
      EXPECT_EQ(hash_value(a), hash_value(b));
    }
    if (hash_value(a) != hash_value(b)) {
      EXPECT_NE(a, b);
    }
    // Some LLVM containers use "!=".
    EXPECT_EQ(a != b, !(a == b));
  };

  std::vector<Interval> intervals = {Interval{1, 1}, Interval{0, 1},
                                     Interval{1, 2}};
  for (const auto& a : intervals) {
    for (const auto& b : intervals) {
      check_consistent(a, b);
    }
  }
}

TEST_F(IndexingMapTest, DimVarSupportsAbslHashAndEqAndNe) {
  ExpectSupportsAbslHashAndEqAndNe<DimVar>(
      {DimVar{1, 1}, DimVar{0, 1}, DimVar{1, 2}});
}

TEST_F(IndexingMapTest, RangeVarSupportsAbslHashAndEqAndNe) {
  ExpectSupportsAbslHashAndEqAndNe<RangeVar>(
      {RangeVar{1, 1}, RangeVar{0, 1}, RangeVar{1, 2}});
}

TEST_F(IndexingMapTest, RTVarSupportsAbslHashAndEqAndNe) {
  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<VerifiedHloModule> hlo_module,
                          ParseAndReturnVerifiedModule(R"(
HloModule m

ENTRY e {
  ROOT %constant = s64[] constant(42)
})"));
  ASSERT_NE(hlo_module, nullptr);
  const HloInstruction* constant_instr =
      hlo_module->entry_computation()->root_instruction();

  ExpectSupportsAbslHashAndEqAndNe<RTVar>(
      {RTVar{Interval{1, 1}, nullptr,
             ParseAffineMap("(d0) -> (d0)", &mlir_context_)},
       RTVar{Interval{1, 2}, nullptr,
             ParseAffineMap("(d0) -> (d0)", &mlir_context_)},
       RTVar{
           Interval{1, 2},
           nullptr,
           ParseAffineMap("(d0) -> (d0 * 2)", &mlir_context_),
       },
       RTVar{
           Interval{1, 2},
           constant_instr,
           ParseAffineMap("(d0) -> (d0 * 2)", &mlir_context_),
       }});
}

TEST_F(IndexingMapTest, IndexingMapSupportsAbslHashAndEqAndNe) {
  auto zero_dim_map = AffineMap::get(&mlir_context_);
  ExpectSupportsAbslHashAndEqAndNe<IndexingMap>(
      {IndexingMap::FromTensorSizes(
           ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1, s0)",
                          &mlir_context_),
           {50, 60}, {70, 80}),
       IndexingMap::FromTensorSizes(
           ParseAffineMap("(d0, d1)[s0, s1] -> (d1 * 2, d0, s1, s0)",
                          &mlir_context_),
           {50, 60}, {70, 80}),
       IndexingMap::FromTensorSizes(
           ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1, s0)",
                          &mlir_context_),
           {51, 60}, {70, 80}),
       IndexingMap::FromTensorSizes(
           ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1, s0)",
                          &mlir_context_),
           {50, 60}, {71, 80}),
       [&] {
         auto m = IndexingMap::FromTensorSizes(
             ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1, s0)",
                            &mlir_context_),
             {50, 60}, {70, 80});
         m.AddConstraint(ParseAffineExpr("d0 mod 8", &mlir_context_),
                         Interval{0, 0});
         m.AddConstraint(ParseAffineExpr("d0 mod 16", &mlir_context_),
                         Interval{0, 0});
         return m;
       }(),
       [&] {
         auto m = IndexingMap::FromTensorSizes(
             ParseAffineMap("(d0, d1)[s0, s1] -> (d1, d0, s1, s0)",
                            &mlir_context_),
             {50, 60}, {70, 80});
         m.AddConstraint(ParseAffineExpr("d0 mod 8", &mlir_context_),
                         Interval{0, 0});
         m.AddConstraint(ParseAffineExpr("d0 mod 32", &mlir_context_),
                         Interval{0, 0});
         return m;
       }(),
       IndexingMap(
           ParseAffineMap("(d0)[s0, s1, s2, s3, s4] -> (d0 * 4 + s1 + s3 - 42)",
                          &mlir_context_),
           {DimVar{{0, 31}}},
           {RangeVar{{0, 0}}, RangeVar{{0, 1}}, RangeVar{{0, 2}}},
           {RTVar{Interval{0, 3},
                  /*instr=*/nullptr, zero_dim_map},
            RTVar{Interval{0, 4},
                  /*instr=*/nullptr, zero_dim_map}}),
       IndexingMap(
           ParseAffineMap("(d0)[s0, s1, s2, s3, s4] -> (d0 * 4 + s1 + s3 - 42)",
                          &mlir_context_),
           {DimVar{{0, 31}}},
           {RangeVar{{0, 0}}, RangeVar{{0, 1}}, RangeVar{{0, 2}}},
           {RTVar{Interval{0, 3},
                  /*instr=*/nullptr, zero_dim_map},
            RTVar{Interval{0, 5},
                  /*instr=*/nullptr, zero_dim_map}})});
}

}  // namespace
}  // namespace gpu
}  // namespace xla

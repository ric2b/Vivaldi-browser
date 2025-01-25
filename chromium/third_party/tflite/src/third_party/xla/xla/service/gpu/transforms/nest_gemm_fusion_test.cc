/* Copyright 2024 The OpenXLA Authors.

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

#include "xla/service/gpu/transforms/nest_gemm_fusion.h"

#include <ostream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/service/gpu/backend_configs.pb.h"
#include "xla/service/pattern_matcher.h"
#include "xla/service/pattern_matcher_gmock.h"
#include "xla/tests/hlo_test_base.h"
#include "xla/tsl/lib/core/status_test_util.h"
#include "tsl/platform/statusor.h"

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::tsl::testing::IsOkAndHolds;
using ::tsl::testing::StatusIs;

namespace xla {

// Gtest hook to pretty-print an HloInstruction.
static void PrintTo(const HloInstruction& hlo, std::ostream* os) {
  *os << hlo.ToString();
}

namespace gpu {
namespace {

// Wraps a matcher for a fusion instruction's output tile sizes.
// Proto matchers would be nice, but b/229726259 is P2.
MATCHER_P(OutputTileSizesIs, matcher, "") {
  auto backend_config = arg.template backend_config<GpuBackendConfig>();
  if (!backend_config.ok()) {
    *result_listener << "failed to get backend config: "
                     << backend_config.status();
    return false;
  }
  FusionBackendConfig fusion_backend_config =
      backend_config->fusion_backend_config();
  if (!fusion_backend_config.has_block_level_fusion_config()) {
    *result_listener << "has no block level fusion config";
    return false;
  }
  auto output_tile_sizes =
      fusion_backend_config.block_level_fusion_config().output_tile_sizes();
  return ExplainMatchResult(matcher, output_tile_sizes, result_listener);
}

class NestGemmFusionTest : public HloTestBase {};

TEST_F(NestGemmFusionTest, BasicTest) {
  absl::string_view hlo = R"(
dot {
  lhs = bf16[8192,512] parameter(0)
  rhs = bf16[512,512] parameter(1)
  ROOT  dot = bf16[8192,512] dot(lhs, rhs),
    lhs_contracting_dims={1}, rhs_contracting_dims={0}
}

ENTRY entry {
  p0 = bf16[8192,512] parameter(0)
  p1 = bf16[512,512] parameter(1)
  ROOT fusion = bf16[8192,512] fusion(p0, p1),
    kind=kCustom, calls=dot, backend_config={
      "fusion_backend_config": {
        "kind":"__triton_gemm",  "triton_gemm_config": {
          "block_m":"64", "block_n":"256", "block_k":"32",
          "split_k":"1", "num_stages":"1", "num_warps":"1", "num_ctas":"1"
        }
      }
    }
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(hlo));
  EXPECT_THAT(NestGemmFusion().Run(module.get()), IsOkAndHolds(true));
  TF_ASSERT_OK(verifier().Run(module.get()).status());

  const HloInstruction* fusion = nullptr;
  ASSERT_THAT(module->entry_computation()->root_instruction(),
              GmockMatch(match::Fusion(&fusion)));
  EXPECT_THAT(*fusion, OutputTileSizesIs(ElementsAre(64, 256)));

  const HloInstruction* lhs = nullptr;
  const HloInstruction* rhs = nullptr;
  EXPECT_THAT(fusion->fused_expression_root(),
              GmockMatch(match::Dot(match::Fusion(&lhs), match::Fusion(&rhs))));
  EXPECT_THAT(*lhs, OutputTileSizesIs(ElementsAre(64, 32)));
  EXPECT_THAT(*rhs, OutputTileSizesIs(ElementsAre(32, 256)));
}

// Tests hoisting of bitcasts which would otherwise trigger unsatisfiable
// constraints during symbolic tile analysis.
TEST_F(NestGemmFusionTest, BitcastsAreHoistedOutOfGemmFusions) {
  absl::string_view hlo = R"(
dot {
  lhs = f32[21] parameter(0)
  bitcast = f32[3,7]{0,1} bitcast(lhs)
  rhs = f32[7,11] parameter(1)
  ROOT dot = f32[3,11] dot(bitcast, rhs),
    lhs_contracting_dims={1}, rhs_contracting_dims={0}
}

ENTRY entry {
  p0 = f32[21] parameter(0)
  p1 = f32[7,11] parameter(1)
  ROOT fusion = f32[3,11] fusion(p0, p1),
    kind=kCustom, calls=dot, backend_config={
      "fusion_backend_config": {
        "kind":"__triton_gemm",  "triton_gemm_config": {
          "block_m":"32", "block_n":"64", "block_k":"16",
          "split_k":"1", "num_stages":"1", "num_warps":"1", "num_ctas":"1"
        }
      }
    }
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(hlo));
  EXPECT_THAT(NestGemmFusion().Run(module.get()), IsOkAndHolds(true));
  TF_ASSERT_OK(verifier().Run(module.get()).status());

  const HloInstruction* fusion = nullptr;
  ASSERT_THAT(module->entry_computation()->root_instruction(),
              GmockMatch(match::Fusion(&fusion)));
  EXPECT_THAT(fusion->operand(0), GmockMatch(match::Bitcast()));
  EXPECT_THAT(*fusion, OutputTileSizesIs(ElementsAre(32, 64)));

  const HloInstruction* lhs = nullptr;
  const HloInstruction* rhs = nullptr;
  EXPECT_THAT(fusion->fused_expression_root(),
              GmockMatch(match::Dot(match::Fusion(&lhs), match::Fusion(&rhs))));
  EXPECT_THAT(*lhs, OutputTileSizesIs(ElementsAre(32, 16)));
  EXPECT_THAT(*rhs, OutputTileSizesIs(ElementsAre(16, 64)));
}

TEST_F(NestGemmFusionTest, FailsOnBitcastWithOpenProducerSet) {
  absl::string_view hlo = R"(
dot {
  p0 = f32[32] parameter(0)
  lhs = f32[4,8] bitcast(p0)
  rhs = f32[8,4] bitcast(p0)
  ROOT dot = f32[4,4] dot(lhs, rhs),
    lhs_contracting_dims={1}, rhs_contracting_dims={0}
}

ENTRY entry {
  p0 = f32[32] parameter(0)
  ROOT fusion = f32[4,4] fusion(p0),
    kind=kCustom, calls=dot, backend_config={
      "fusion_backend_config": {
        "kind":"__triton_gemm",  "triton_gemm_config": {
          "block_m":"4", "block_n":"4", "block_k":"8",
          "split_k":"1", "num_stages":"1", "num_warps":"1", "num_ctas":"1"
        }
      }
    }
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(hlo));
  EXPECT_THAT(NestGemmFusion().Run(module.get()).status(),
              StatusIs(absl::StatusCode::kFailedPrecondition,
                       HasSubstr("not in the producer set")));
}

TEST_F(NestGemmFusionTest, BitcastsCanBeHoistedPastOtherBitcasts) {
  absl::string_view hlo = R"(
dot {
  lhs = f32[3,7] parameter(0)
  bitcast0 = f32[21] bitcast(lhs)
  bitcast1 = f32[3,7] bitcast(bitcast0)
  rhs = f32[7,11] parameter(1)
  ROOT dot = f32[3,11] dot(bitcast1, rhs),
    lhs_contracting_dims={1}, rhs_contracting_dims={0}
}

ENTRY entry {
  p0 = f32[3, 7] parameter(0)
  p1 = f32[7,11] parameter(1)
  ROOT fusion = f32[3,11] fusion(p0, p1),
    kind=kCustom, calls=dot, backend_config={
      "fusion_backend_config": {
        "kind":"__triton_gemm",  "triton_gemm_config": {
          "block_m":"32", "block_n":"64", "block_k":"16",
          "split_k":"1", "num_stages":"1", "num_warps":"1", "num_ctas":"1"
        }
      }
    }
}
)";

  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(hlo));
  EXPECT_THAT(NestGemmFusion().Run(module.get()), IsOkAndHolds(true));
  TF_ASSERT_OK(verifier().Run(module.get()).status());
}

}  // namespace
}  // namespace gpu
}  // namespace xla

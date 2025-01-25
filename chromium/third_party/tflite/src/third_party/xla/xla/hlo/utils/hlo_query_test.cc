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

#include "xla/hlo/utils/hlo_query.h"

#include <memory>
#include <utility>

#include <gtest/gtest.h>
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_module.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/service/hlo_parser.h"
#include "xla/tests/hlo_test_base.h"
#include "tsl/platform/statusor.h"

namespace xla {
namespace {

using HloQueryTest = HloTestBase;

template <typename Hlo>
int CountInstructions(Hlo& module, HloOpcode opcode) {
  int counter = 0;
  hlo_query::ForEachInstructionWithOpcode(
      module, opcode, [&counter](auto& instr) { counter++; });
  return counter;
}

constexpr absl::string_view kConstantAdditionHloString = R"(
HloModule test
ENTRY main {
  zero = f32[] constant(0)
  five = f32[] constant(5)
  ROOT out = f32[] add(zero, five)
})";

TEST_F(HloQueryTest,
       GetInstructionWithOpCodeReturnsMatchingInstructionForModule) {
  constexpr absl::string_view kHloString = R"(
HloModule m

computation.0 {
  param.0 = f32[32]{0} parameter(0)
  ROOT _ = f32[32]{0} rsqrt(param.0)
}

ENTRY main {
  param.0 = f32[32]{0} parameter(0)
  param.1 = f32[32]{0} parameter(1)
  param.2 = f32[32]{0} parameter(2)
  param.3 = f32[32]{0} parameter(3)
  add.0 = f32[32]{0} add(param.0,param.1)
  add.1 = f32[32]{0} add(param.1,param.2)
  sub.0 = f32[32]{0} subtract(param.0,param.1)
  mul.0 = f32[32]{0} multiply(param.0,param.1)
  mul.1 = f32[32]{0} multiply(param.1,param.2)
  mul.2 = f32[32]{0} multiply(param.2,param.3)
  comp.0 = call(param.0), to_apply=computation.0
  ROOT _ = (f32[32],f32[32],f32[32],f32[32],f32[32],f32[32],f32[32]) tuple(comp.0,add.0,add.1,sub.0,mul.0,mul.1,mul.2)
})";

  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          ParseAndReturnUnverifiedModule(kHloString));
  EXPECT_EQ(CountInstructions(*module, HloOpcode::kAdd), 2);
  EXPECT_EQ(CountInstructions(*module, HloOpcode::kSubtract), 1);
  EXPECT_EQ(CountInstructions(*module, HloOpcode::kMultiply), 3);
}

TEST_F(HloQueryTest,
       GetInstructionWithOpCodeReturnsMatchingInstructionForComputation) {
  constexpr absl::string_view kHloString = R"(
HloModule m

computation.0 {
  param.0 = f32[32]{0} parameter(0)
  param.1 = f32[32]{0} parameter(1)
  param.2 = f32[32]{0} parameter(2)
  param.3 = f32[32]{0} parameter(3)
  add.0 = f32[32]{0} add(param.0,param.1)
  add.1 = f32[32]{0} add(param.1,param.2)
  sub.0 = f32[32]{0} subtract(param.0,param.1)
  mul.0 = f32[32]{0} multiply(param.0,param.1)
  mul.1 = f32[32]{0} multiply(param.1,param.2)
  ROOT mul.2 = f32[32]{0} multiply(param.2,param.3)
}

ENTRY main {
  param.0 = f32[32]{0} parameter(0)
  param.1 = f32[32]{0} parameter(1)
  param.2 = f32[32]{0} parameter(2)
  param.3 = f32[32]{0} parameter(3)
  add.0 = f32[32]{0} add(param.0,param.1)
  sub.0 = f32[32]{0} subtract(param.0,param.1)
  mul.0 = f32[32]{0} multiply(param.0,param.1)
  comp.0 = f32[32]{0} call(param.0,param.1,param.2), to_apply=computation.0
  ROOT _ = (f32[32],f32[32],f32[32],f32[32]) tuple(add.0,sub.0,mul.0,comp.0)
})";

  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          ParseAndReturnUnverifiedModule(kHloString));
  HloComputation* computation = module->GetComputationWithName("computation.0");
  EXPECT_EQ(CountInstructions(*computation, HloOpcode::kAdd), 2);
  EXPECT_EQ(CountInstructions(*computation, HloOpcode::kSubtract), 1);
  EXPECT_EQ(CountInstructions(*computation, HloOpcode::kMultiply), 3);
}

TEST_F(HloQueryTest, GetUniqueGteTest) {
  constexpr absl::string_view kHloString = R"(
  HloModule m

  ENTRY main {
    param.0 = (f32[32]{0}, f32[32]{0}, f32[32]{0}, f32[32]{0}) parameter(0)
    gte1 = f32[32]{0} get-tuple-element(param.0), index=0
    gte2 = f32[32]{0} get-tuple-element(param.0), index=1
    dup_gte2 = f32[32]{0} get-tuple-element(param.0), index=1
    gte3 = f32[32]{0} get-tuple-element(param.0), index=2
    ROOT gte4 = f32[32]{0} get-tuple-element(param.0), index=3
  })";

  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          ParseAndReturnUnverifiedModule(kHloString));
  HloInstruction* param = module->entry_computation()->parameter_instruction(0);
  HloInstruction* gte1 = hlo_query::GetUniqueGteInstruction(param, /*index=*/0);
  EXPECT_NE(gte1, nullptr);
  HloInstruction* gte2 = hlo_query::GetUniqueGteInstruction(param, /*index=*/1);
  EXPECT_EQ(gte2, nullptr);
}

TEST_F(HloQueryTest, FindComputationTest) {
  TF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<HloModule> module,
      ParseAndReturnUnverifiedModule(kConstantAdditionHloString));
  EXPECT_NE(hlo_query::FindComputation(module.get(), "main"), nullptr);
  EXPECT_EQ(hlo_query::FindComputation(module.get(), "foo"), nullptr);
}

TEST_F(HloQueryTest, FindInstructionUsingNameTest) {
  TF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<HloModule> module,
      ParseAndReturnUnverifiedModule(kConstantAdditionHloString));
  const HloComputation* main = hlo_query::FindComputation(module.get(), "main");
  EXPECT_NE(hlo_query::FindFirstInstruction(main, "zero").first, nullptr);
  EXPECT_NE(hlo_query::FindFirstInstruction(main, "five").first, nullptr);
  EXPECT_NE(hlo_query::FindFirstInstruction(main, "out").first, nullptr);
  EXPECT_EQ(hlo_query::FindFirstInstruction(main, "foo").first, nullptr);
}

std::pair<HloInstruction*, int> FindFirst(const HloComputation* main,
                                          absl::string_view opcode) {
  return hlo_query::FindFirstInstruction(main,
                                         StringToHloOpcode(opcode).value());
}

// Assures that the string and opcode versions of FindFirstInstruction return
// the same result
void FindFirstInstructionsAndExpectEqual(const HloComputation* main,
                                         absl::string_view name,
                                         absl::string_view opcode_str) {
  SCOPED_TRACE(absl::StrCat("Comparing finding by name: ", name,
                            " and opcode: ", opcode_str));
  auto withString = hlo_query::FindFirstInstruction(main, name);
  auto withOpCode = FindFirst(main, opcode_str);
  EXPECT_EQ(withString.first, withOpCode.first);
  EXPECT_EQ(withString.second, withOpCode.second);
  if (withString.first != nullptr)
    EXPECT_EQ(withString.first->ToString(), withOpCode.first->ToString());
}

TEST_F(HloQueryTest, FindInstructionUsingOpcodeTest) {
  TF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<HloModule> module,
      ParseAndReturnUnverifiedModule(kConstantAdditionHloString));
  const HloComputation* main = hlo_query::FindComputation(module.get(), "main");
  EXPECT_NE(FindFirst(main, "add").first, nullptr);
  EXPECT_NE(FindFirst(main, "constant").first, nullptr);
  EXPECT_EQ(FindFirst(main, "select").first, nullptr);
}

TEST_F(HloQueryTest, FindInstructionUsingOpcodeAndNameEqualTest) {
  TF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<HloModule> module,
      ParseAndReturnUnverifiedModule(kConstantAdditionHloString));
  const HloComputation* main = hlo_query::FindComputation(module.get(), "main");
  FindFirstInstructionsAndExpectEqual(main, "zero", "constant");
  FindFirstInstructionsAndExpectEqual(main, "out", "add");
  // both are not found
  FindFirstInstructionsAndExpectEqual(main, "dummy", "select");
}

TEST_F(HloQueryTest, FindInstructionDoesNotExistTest) {
  TF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<HloModule> module,
      ParseAndReturnUnverifiedModule(kConstantAdditionHloString));
  const HloComputation* main = hlo_query::FindComputation(module.get(), "main");
  EXPECT_NE(main, nullptr);
  auto find_beef = hlo_query::FindFirstInstruction(main, "deadbeef");
  auto find_nothing = hlo_query::FindFirstInstruction(main, "");
  EXPECT_EQ(find_beef.first, nullptr);
  EXPECT_EQ(find_beef.second, -1);
  EXPECT_EQ(find_nothing.first, nullptr);
  EXPECT_EQ(find_nothing.second, -1);
}

TEST_F(HloQueryTest, IsBeforeInComputationTest) {
  TF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<HloModule> module,
      ParseAndReturnUnverifiedModule(kConstantAdditionHloString));
  const HloComputation* main = hlo_query::FindComputation(module.get(), "main");
  EXPECT_TRUE(hlo_query::IsBeforeInComputation(main, "zero", "five"));
  EXPECT_TRUE(hlo_query::IsBeforeInComputation(main, "five", "out"));
}

TEST_F(HloQueryTest, NextChannelIdForModuleWithoutChannelIdTest) {
  TF_ASSERT_OK_AND_ASSIGN(
      auto module, ParseAndReturnUnverifiedModule(kConstantAdditionHloString));
  EXPECT_EQ(hlo_query::NextChannelId(*module), 1)
      << "module with no channel id";
}

TEST_F(HloQueryTest, NextChannelIdBasicTest) {
  absl::string_view hlo = R"(
    HloModule test
    ENTRY test_computation {
      p = u32[] partition-id()
      ROOT start = u32[] collective-permute(p), channel_id=8,
        source_target_pairs={{0,1},{1,2},{2,3},{3,0}}
    }
    )";
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnUnverifiedModule(hlo));
  EXPECT_EQ(hlo_query::NextChannelId(*module), 9);
}

TEST_F(HloQueryTest, NextChannelIdTwoIdsTest) {
  absl::string_view hlo = R"(
    HloModule test
    ENTRY test_computation {
      p = u32[] partition-id()
      l = u32[] collective-permute(p), channel_id=8, source_target_pairs={{0,1},{1,2}}
      r = u32[] collective-permute(p), channel_id=9, source_target_pairs={{2,3},{3,0}}
      ROOT res = u32[] add(l,r)
    }
    )";
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnUnverifiedModule(hlo));
  EXPECT_EQ(hlo_query::NextChannelId(*module), 10);
}

}  // namespace
}  // namespace xla

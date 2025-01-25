/* Copyright 2020 The OpenXLA Authors.

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

#include "xla/service/conditional_canonicalizer.h"

#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_module.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/hlo/utils/hlo_matchers.h"
#include "xla/service/hlo_parser.h"
#include "xla/shape_util.h"
#include "xla/tests/hlo_test_base.h"
#include "xla/tests/literal_test_util.h"
#include "xla/tests/test_utils.h"
#include "xla/tsl/lib/core/status_test_util.h"
#include "xla/types.h"
#include "xla/util.h"

namespace xla {
namespace {

namespace op = xla::testing::opcode_matchers;

class ConditionalCanonicalizerTest : public HloTestBase {
 protected:
  ConditionalCanonicalizerTest() {}
};

TEST_F(ConditionalCanonicalizerTest, DenseArrayConditionalRewrite) {
  auto module = ParseAndReturnVerifiedModule(R"(
HloModule _
true_branch {
  true_param = (s32[3,2]) parameter(0)
  ROOT root = s32[] constant(0)
}

false_branch {
  false_param = (s32[3,2]) parameter(0)
  ROOT root = s32[] constant(1)
}

ENTRY entry {
  param0 = s32[3,2] parameter(0)
  branch = pred[] constant(false)
  param_tuple = (s32[3 ,2]) tuple(param0)
  ROOT conditional = s32[] conditional(branch, param_tuple, param_tuple),
    true_computation=true_branch, false_computation=false_branch
}
)")
                    .value();
  ConditionalCanonicalizer pass;
  EXPECT_TRUE(pass.Run(module.get()).value());
  EXPECT_THAT(module->entry_computation()->root_instruction(),
              op::GetTupleElement(op::Conditional()));
}

}  // namespace
}  // namespace xla

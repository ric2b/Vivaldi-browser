/* Copyright 2022 The OpenXLA Authors.

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

#include "xla/hlo/experimental/auto_sharding/auto_sharding_util.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "json/json.h"
#include "xla/array.h"
#include "xla/hlo/experimental/auto_sharding/auto_sharding_strategy.h"
#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_input_output_alias_config.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/hlo/ir/hlo_schedule.h"
#include "xla/hlo/ir/hlo_sharding.h"
#include "xla/hlo/ir/ptrvec.h"
#include "xla/hlo/utils/hlo_sharding_util.h"
#include "xla/service/call_graph.h"
#include "xla/service/sharding_propagation.h"
#include "xla/service/while_loop_analysis.h"
#include "xla/shape.h"
#include "xla/shape_tree.h"
#include "xla/shape_util.h"
#include "xla/xla_data.pb.h"
#include "tsl/platform/errors.h"

namespace xla {
namespace spmd {

inline const HloInstruction* PassThroughCustomCallMarkerGetSource(
    const HloInstruction* ins);
inline HloInstruction* PassThroughCustomCallMarkerUser(
    HloInstruction* raw_user, const HloInstruction* inst);

std::optional<HloSharding> GetInputSharding(const HloInstruction* ins,
                                            int64_t op_index,
                                            const HloSharding& output_sharding,
                                            const CallGraph& call_graph,
                                            int64_t num_devices) {
  std::unique_ptr<HloInstruction> ins_clone = ins->Clone();
  ins_clone->set_sharding(output_sharding);

  std::vector<std::unique_ptr<HloInstruction>> operands;
  for (size_t i = 0; i < ins->operand_count(); ++i) {
    const HloInstruction* operand = ins->operand(i);
    if (i != op_index &&
        (!operand->has_sharding() ||
         operand->sharding().Validate(operand->shape(), num_devices).ok())) {
      continue;
    }
    std::unique_ptr<HloInstruction> operand_clone = operand->Clone();
    if (operand_clone->has_sharding() &&
        !operand_clone->sharding()
             .Validate(operand_clone->shape(), num_devices)
             .ok()) {
      operand_clone->clear_sharding();
    }
    CHECK_OK(ins_clone->ReplaceOperandWith(i, operand_clone.get()));
    operands.push_back(std::move(operand_clone));
  }

  std::optional<HloSharding> inferred_sharding =
      ShardingPropagation::GetShardingFromUser(*ins_clone->operand(op_index),
                                               *ins_clone, 10, true, call_graph,
                                               /*sharding_helper=*/nullptr);

  if (!inferred_sharding.has_value() && IsTopKCustomCall(ins)) {
    // ShardingPropagation::GetShardingFromUser does not handle TopK custom
    // calls. Mirroring that function's handling of kSort, we handle TopK below.
    inferred_sharding = output_sharding;
  }
  return inferred_sharding;
}

// Return whether the instruction is an activation from another pipeline stage.
bool IsActivationFromAnotherStage(const HloInstruction* ins,
                                  const InstructionBatchDimMap& batch_dim_map) {
  if (!(ins->opcode() == HloOpcode::kParameter &&
        batch_dim_map.contains(GetBatchDimMapKey(ins)))) {
    return false;
  }

  for (const HloInstruction* user : ins->users()) {
    if (!(user->opcode() == HloOpcode::kTuple && user->users().size() == 1 &&
          user->users().front()->IsCustomCall(kPipelineMarker) &&
          absl::StrContains(user->users().front()->metadata().op_type(),
                            "start"))) {
      return false;
    }
  }

  return true;
}

// Propagate sharding for dim-wise operations (e.g., slice, pad) which works
// independently on each dimension.
// The sharding can successfully propagate if the operation only happens
// on tensor dimensions that are not tiled.
std::optional<HloSharding> PropagateDimwiseSharding(
    const HloSharding& input_spec, const Shape& old_shape,
    const Shape& new_shape) {
  if (input_spec.IsReplicated()) {
    return input_spec;
  }

  CHECK(old_shape.IsArray());

  const auto& tile_assignment = input_spec.tile_assignment();
  for (int64_t i = 0; i < old_shape.rank(); ++i) {
    if (tile_assignment.dim(i) > 1 &&
        new_shape.dimensions(i) != old_shape.dimensions(i)) {
      return std::nullopt;
    }
  }

  return input_spec;
}

// Propagate sharding for ReduceWindow-like operations.
// The sharding can successfully propagate if the window operation only happens
// on tensor dimensions that are not tiled.
std::optional<HloSharding> PropagateReduceWindowSharding(
    const HloSharding& input_spec, const Shape& old_shape,
    const Window& window) {
  if (input_spec.IsReplicated()) {
    return input_spec;
  }

  CHECK(!input_spec.IsTuple());

  const auto& tile_assignment = input_spec.tile_assignment();
  for (int64_t i = 0; i < old_shape.rank(); ++i) {
    if (tile_assignment.dim(i) > 1 && window.dimensions(i).size() != 1) {
      return std::nullopt;
    }
  }

  return input_spec;
}

// Depth analysis (breadth first search).
// We also assign a much larger distance to heavy operators (e.g., dot,
// convolution).
InstructionDepthMap BuildInstructionDepthMap(
    const HloInstructionSequence& sequence,
    const InstructionBatchDimMap& batch_dim_map) {
  const std::vector<HloInstruction*>& instructions = sequence.instructions();

  InstructionDepthMap depth_map;
  StableHashMap<const HloInstruction*, size_t> degree_dict;

  // Init frontier
  size_t collected = 0;
  std::vector<const HloInstruction*> current_frontier;
  for (const HloInstruction* inst : instructions) {
    degree_dict[inst] = inst->unique_operands().size();
    if (degree_dict[inst] == 0) {
      depth_map[inst] = 0;

      // Add some initial depth for activations from other pipeline stages.
      if (IsActivationFromAnotherStage(inst, batch_dim_map)) {
        depth_map[inst] = 20;
      }

      current_frontier.push_back(inst);
      collected++;
    }
  }

  // Push forward
  std::vector<const HloInstruction*> next_frontier;
  while (collected < instructions.size()) {
    CHECK(!current_frontier.empty());
    next_frontier.clear();
    for (const HloInstruction* inst : current_frontier) {
      for (const HloInstruction* node : inst->users()) {
        int now_degree = --degree_dict[node];
        if (now_degree == 0) {
          int64_t delta = 0;
          bool reset = false;

          // Heavy operators have more weight (distance).
          switch (node->opcode()) {
            case HloOpcode::kDot:
            case HloOpcode::kConvolution:
              delta = 1000;
              break;
            // A temporary hack here: reduce ops will generate replicated
            // sharding. We do not want the later broadcast and elementwise ops
            // to follow it. So we give reduce ops some penalty and let the
            // elementwise ops to follow other operands.
            // TODO(zhuohan): remove this hack by correctly registering
            // strategies for broadcast.
            case HloOpcode::kReduce:
              reset = true;
              break;
            // For similar reasons mentioned above, we give some penalty to
            // broadcast.
            case HloOpcode::kBroadcast:
              delta = -5;
              break;
            case HloOpcode::kReshape:
              delta = 0;
              break;
            default:
              delta = 1;
              break;
          }

          if (reset) {
            depth_map[node] = 0;
          } else if (node->opcode() == HloOpcode::kGetTupleElement &&
                     IsCustomCallMarker(node->operand(0))) {
            depth_map[node] =
                depth_map.at(PassThroughCustomCallMarkerGetSource(node));
          } else {
            int64_t max_depth = depth_map.at(inst) + delta;
            for (const HloInstruction* operand : node->operands()) {
              max_depth = std::max(max_depth, depth_map.at(operand) + delta);
            }
            depth_map[node] = max_depth;
          }

          next_frontier.push_back(node);
          collected += 1;
        }
      }
    }

    std::swap(current_frontier, next_frontier);
  }

  return depth_map;
}

std::string GetBatchDimMapKey(const HloInstruction* ins, int64_t idx) {
  if (idx >= 0) {
    return absl::StrCat(ins->name(), "/", idx);
  }
  return std::string(ins->name());
}

void BatchDimMapForward(const std::vector<HloInstruction*>& instructions,
                        InstructionBatchDimMap& batch_map) {
  for (const HloInstruction* ins : instructions) {
    switch (ins->opcode()) {
      case HloOpcode::kParameter:
      case HloOpcode::kConstant:
      case HloOpcode::kIota:
      case HloOpcode::kRngGetAndUpdateState:
      case HloOpcode::kRng:
      case HloOpcode::kRngBitGenerator:
      case HloOpcode::kGather:
        // TODO(b/220935014) Shard kGather properly.
        break;
      case HloOpcode::kBroadcast: {
        const HloInstruction* operand = ins->operand(0);
        const auto& dimensions = ins->dimensions();

        if (batch_map.contains(GetBatchDimMapKey(operand))) {
          int value = batch_map[GetBatchDimMapKey(operand)];
          int old_dim = -1;
          for (int i = 0; i < ins->shape().rank(); ++i) {
            if (absl::c_linear_search(dimensions, i)) {
              old_dim++;
            }

            if (old_dim == value) {
              batch_map[GetBatchDimMapKey(ins)] = i;
              break;
            }
          }
        }
        break;
      }
      case HloOpcode::kReshape: {
        const HloInstruction* operand = ins->operand(0);
        if (batch_map.contains(GetBatchDimMapKey(operand))) {
          int value = batch_map[GetBatchDimMapKey(operand)];
          bool match = true;
          for (int i = 0; i < value; ++i) {
            if (operand->shape().dimensions(i) != ins->shape().dimensions(i)) {
              match = false;
              break;
            }
          }

          if (match) {
            batch_map[GetBatchDimMapKey(ins)] = value;
          }
        }
        break;
      }
      case HloOpcode::kTranspose: {
        const HloInstruction* operand = ins->operand(0);
        const auto& dimensions = ins->dimensions();

        if (batch_map.contains(GetBatchDimMapKey(operand))) {
          int value = batch_map[GetBatchDimMapKey(operand)];
          auto it = absl::c_find(dimensions, value);
          batch_map[GetBatchDimMapKey(ins)] = it - dimensions.begin();
        }
        break;
      }
      case HloOpcode::kReverse:
      case HloOpcode::kPad:
      case HloOpcode::kSlice:
      case HloOpcode::kConcatenate:
      case HloOpcode::kDynamicSlice:
      case HloOpcode::kDynamicUpdateSlice:
      case HloOpcode::kReduceWindow:
      case HloOpcode::kSelectAndScatter:
      // Unary elementwise operations.
      case HloOpcode::kAbs:
      case HloOpcode::kRoundNearestAfz:
      case HloOpcode::kRoundNearestEven:
      case HloOpcode::kCeil:
      case HloOpcode::kClz:
      case HloOpcode::kConvert:
      case HloOpcode::kBitcastConvert:
      case HloOpcode::kCopy:
      case HloOpcode::kCos:
      case HloOpcode::kErf:
      case HloOpcode::kExp:
      case HloOpcode::kExpm1:
      case HloOpcode::kFloor:
      case HloOpcode::kImag:
      case HloOpcode::kIsFinite:
      case HloOpcode::kLog:
      case HloOpcode::kLog1p:
      case HloOpcode::kNot:
      case HloOpcode::kNegate:
      case HloOpcode::kPopulationCount:
      case HloOpcode::kReal:
      case HloOpcode::kReducePrecision:
      case HloOpcode::kRsqrt:
      case HloOpcode::kLogistic:
      case HloOpcode::kSign:
      case HloOpcode::kSin:
      case HloOpcode::kSqrt:
      case HloOpcode::kCbrt:
      case HloOpcode::kTan:
      case HloOpcode::kTanh:
      // Binary elementwise operations
      case HloOpcode::kAdd:
      case HloOpcode::kAtan2:
      case HloOpcode::kCompare:
      case HloOpcode::kComplex:
      case HloOpcode::kDivide:
      case HloOpcode::kMaximum:
      case HloOpcode::kMinimum:
      case HloOpcode::kMultiply:
      case HloOpcode::kPower:
      case HloOpcode::kRemainder:
      case HloOpcode::kSubtract:
      case HloOpcode::kAnd:
      case HloOpcode::kOr:
      case HloOpcode::kXor:
      case HloOpcode::kShiftLeft:
      case HloOpcode::kShiftRightArithmetic:
      case HloOpcode::kShiftRightLogical:
      case HloOpcode::kStochasticConvert:
      // Ternary elementwise operations.
      case HloOpcode::kSelect:
      case HloOpcode::kClamp: {
        for (const HloInstruction* operand : ins->unique_operands()) {
          if (batch_map.contains(GetBatchDimMapKey(operand))) {
            batch_map[GetBatchDimMapKey(ins)] =
                batch_map[GetBatchDimMapKey(operand)];
            break;
          }
        }
        break;
      }
      case HloOpcode::kReduce: {
        const HloInstruction* operand = ins->operand(0);
        const auto& dimensions = ins->dimensions();

        if (batch_map.contains(GetBatchDimMapKey(operand))) {
          int value = batch_map[GetBatchDimMapKey(operand)];
          if (value == 0 && !absl::c_linear_search(dimensions, value)) {
            batch_map[GetBatchDimMapKey(ins)] = value;
          }
        }
        break;
      }
      case HloOpcode::kDot: {
        const HloInstruction* lhs = ins->operand(0);
        const HloInstruction* rhs = ins->operand(1);
        const auto& dot_dnums = ins->dot_dimension_numbers();
        int64_t space_base_dim = dot_dnums.lhs_batch_dimensions_size();
        const auto& lhs_batch_dims =
            ins->dot_dimension_numbers().lhs_batch_dimensions();
        const auto& rhs_batch_dims =
            ins->dot_dimension_numbers().rhs_batch_dimensions();
        tsl::protobuf::RepeatedField<int64_t> lhs_space_dims, rhs_space_dims;
        std::tie(lhs_space_dims, rhs_space_dims) =
            GetSpaceDims(lhs->shape(), rhs->shape(), dot_dnums);
        // This part assumes that the dot has been through the dot decomposer,
        // which assumes it only includes only one contracting dimension and
        // one non-contracting dimension for both lhs and rhs. Given this
        // assumption, the batch dimension of the dot operator can be determined
        // as in the following cases:
        //   C[b, i, j] += A[b, i, k] * B[b, k, j]
        //   where the batch dimension b is the batch dimension.
        //   C[b, j] += A[b, k] * B[k, j]
        //   where the batch dimension is the non-contracting dimension of A
        //   C[i, b] += A[i, k] * B[k, b]
        //   where the batch dimension is the non-contracting dimension of B
        if (batch_map.contains(GetBatchDimMapKey(lhs))) {
          int value = batch_map[GetBatchDimMapKey(lhs)];
          for (int i = 0; i < lhs_batch_dims.size(); ++i) {
            if (value == lhs_batch_dims[i]) {
              batch_map[GetBatchDimMapKey(ins)] = i;
              break;
            }
          }
          if (value == lhs_space_dims[0]) {
            batch_map[GetBatchDimMapKey(ins)] = space_base_dim;
          }
        }

        if (batch_map.contains(GetBatchDimMapKey(rhs))) {
          int value = batch_map[GetBatchDimMapKey(rhs)];
          for (int i = 0; i < rhs_batch_dims.size(); ++i) {
            if (value == rhs_batch_dims[i]) {
              batch_map[GetBatchDimMapKey(ins)] = i;
              break;
            }
          }
          if (value == rhs_space_dims[0]) {
            batch_map[GetBatchDimMapKey(ins)] = space_base_dim + 1;
          }
        }
        break;
      }
      case HloOpcode::kConvolution: {
        const HloInstruction* lhs = ins->operand(0);
        const HloInstruction* rhs = ins->operand(1);
        const auto& conv_dnums = ins->convolution_dimension_numbers();
        // TODO (zhuohan): Spatial dimension of the convolution may also be
        //   batch dimension.
        // Follow similar logic with Dot, where the input batch dimension or
        // the kernel output feature dimension may be the batch dimension.
        if (batch_map.contains(GetBatchDimMapKey(lhs))) {
          int value = batch_map[GetBatchDimMapKey(lhs)];
          if (value == conv_dnums.input_batch_dimension()) {
            batch_map[GetBatchDimMapKey(ins)] =
                conv_dnums.output_batch_dimension();
          }
        }

        if (batch_map.contains(GetBatchDimMapKey(rhs))) {
          int value = batch_map[GetBatchDimMapKey(rhs)];
          if (value == conv_dnums.kernel_output_feature_dimension()) {
            batch_map[GetBatchDimMapKey(ins)] =
                conv_dnums.output_feature_dimension();
          }
        }
        break;
      }
      case HloOpcode::kGetTupleElement: {
        const HloInstruction* op = ins->operand(0);
        if (batch_map.contains(GetBatchDimMapKey(op, ins->tuple_index()))) {
          batch_map[GetBatchDimMapKey(ins)] =
              batch_map[GetBatchDimMapKey(op, ins->tuple_index())];
        }
        break;
      }
      case HloOpcode::kTuple:
        for (size_t i = 0; i < ins->operand_count(); ++i) {
          const HloInstruction* op = ins->operand(i);
          if (batch_map.contains(GetBatchDimMapKey(op))) {
            batch_map[GetBatchDimMapKey(ins, i)] =
                batch_map[GetBatchDimMapKey(op)];
          }
        }
        break;
      case HloOpcode::kWhile: {
        const HloInstruction* op = ins->operand(0);
        for (size_t i = 0; i < op->shape().tuple_shapes_size(); ++i) {
          if (batch_map.contains(GetBatchDimMapKey(op, i))) {
            batch_map[GetBatchDimMapKey(ins, i)] =
                batch_map[GetBatchDimMapKey(op, i)];
            batch_map[GetBatchDimMapKey(ins->while_body()->root_instruction(),
                                        i)] =
                batch_map[GetBatchDimMapKey(op, i)];
            batch_map[GetBatchDimMapKey(
                ins->while_body()->parameter_instruction(0), i)] =
                batch_map[GetBatchDimMapKey(op, i)];
            batch_map[GetBatchDimMapKey(
                ins->while_condition()->parameter_instruction(0), i)] =
                batch_map[GetBatchDimMapKey(op, i)];
          }
        }
        break;
      }
      case HloOpcode::kCustomCall:
        break;
      default:
        LOG(FATAL) << "Unhandled instruction: " << ins->ToString();
    }
  }
}

void BatchDimMapBackward(const std::vector<HloInstruction*>& instructions,
                         InstructionBatchDimMap& batch_map) {
  for (int64_t i = instructions.size() - 1; i >= 0; i--) {
    const HloInstruction* ins = instructions[i];
    switch (ins->opcode()) {
      case HloOpcode::kBroadcast: {
        const HloInstruction* operand = ins->operand(0);
        const auto& dimensions = ins->dimensions();

        if (batch_map.contains(GetBatchDimMapKey(ins)) &&
            !batch_map.contains(GetBatchDimMapKey(operand))) {
          int value = batch_map[GetBatchDimMapKey(ins)];
          int old_dim = -1;
          for (int i = 0; i < ins->shape().rank(); ++i) {
            if (absl::c_linear_search(dimensions, i)) {
              old_dim++;
            }

            if (i == value && old_dim >= 0) {
              batch_map[GetBatchDimMapKey(operand)] = old_dim;
              break;
            }
          }
        }
        break;
      }
      case HloOpcode::kReshape: {
        const HloInstruction* operand = ins->operand(0);

        if (batch_map.contains(GetBatchDimMapKey(ins)) &&
            !batch_map.contains(GetBatchDimMapKey(operand))) {
          int value = batch_map[GetBatchDimMapKey(ins)];
          bool match = true;
          for (int i = 0; i < value; ++i) {
            if (operand->shape().dimensions(i) != ins->shape().dimensions(i)) {
              match = false;
              break;
            }
          }

          if (match) {
            batch_map[GetBatchDimMapKey(operand)] = value;
          }
        }
        break;
      }
      case HloOpcode::kTranspose: {
        const HloInstruction* operand = ins->operand(0);
        const auto& dimensions = ins->dimensions();

        if (batch_map.contains(GetBatchDimMapKey(ins)) &&
            !batch_map.contains(GetBatchDimMapKey(operand))) {
          batch_map[GetBatchDimMapKey(operand)] =
              dimensions[batch_map[GetBatchDimMapKey(ins)]];
        }
        break;
      }
      case HloOpcode::kReverse:
      case HloOpcode::kPad:
      case HloOpcode::kSlice:
      case HloOpcode::kConcatenate:
      case HloOpcode::kDynamicSlice:
      case HloOpcode::kDynamicUpdateSlice:
      case HloOpcode::kReduceWindow:
      case HloOpcode::kSelectAndScatter:
        // TODO(zhuohan): support these
        break;
      // Unary elementwise operations.
      case HloOpcode::kAbs:
      case HloOpcode::kRoundNearestAfz:
      case HloOpcode::kRoundNearestEven:
      case HloOpcode::kCeil:
      case HloOpcode::kClz:
      case HloOpcode::kConvert:
      case HloOpcode::kBitcastConvert:
      case HloOpcode::kCopy:
      case HloOpcode::kCos:
      case HloOpcode::kErf:
      case HloOpcode::kExp:
      case HloOpcode::kExpm1:
      case HloOpcode::kFloor:
      case HloOpcode::kImag:
      case HloOpcode::kIsFinite:
      case HloOpcode::kLog:
      case HloOpcode::kLog1p:
      case HloOpcode::kNot:
      case HloOpcode::kNegate:
      case HloOpcode::kPopulationCount:
      case HloOpcode::kReal:
      case HloOpcode::kReducePrecision:
      case HloOpcode::kRsqrt:
      case HloOpcode::kLogistic:
      case HloOpcode::kSign:
      case HloOpcode::kSin:
      case HloOpcode::kSqrt:
      case HloOpcode::kCbrt:
      case HloOpcode::kTan:
      case HloOpcode::kTanh:
      // Binary elementwise operations
      case HloOpcode::kAdd:
      case HloOpcode::kAtan2:
      case HloOpcode::kCompare:
      case HloOpcode::kComplex:
      case HloOpcode::kDivide:
      case HloOpcode::kMaximum:
      case HloOpcode::kMinimum:
      case HloOpcode::kMultiply:
      case HloOpcode::kPower:
      case HloOpcode::kRemainder:
      case HloOpcode::kSubtract:
      case HloOpcode::kAnd:
      case HloOpcode::kOr:
      case HloOpcode::kXor:
      case HloOpcode::kShiftLeft:
      case HloOpcode::kShiftRightArithmetic:
      case HloOpcode::kShiftRightLogical:
      case HloOpcode::kStochasticConvert:
      // Ternary elementwise operations.
      case HloOpcode::kSelect:
      case HloOpcode::kClamp: {
        if (batch_map.contains(GetBatchDimMapKey(ins))) {
          int value = batch_map[GetBatchDimMapKey(ins)];
          for (const HloInstruction* operand : ins->unique_operands()) {
            if (!batch_map.contains(GetBatchDimMapKey(operand))) {
              batch_map[GetBatchDimMapKey(operand)] = value;
            }
          }
        }
        break;
      }
      case HloOpcode::kReduce: {
        const HloInstruction* operand = ins->operand(0);
        const auto& dimensions = ins->dimensions();

        if (batch_map.contains(GetBatchDimMapKey(ins)) &&
            !batch_map.contains(GetBatchDimMapKey(operand))) {
          int value = batch_map[GetBatchDimMapKey(ins)];
          if (value == 0 && !absl::c_linear_search(dimensions, value)) {
            batch_map[GetBatchDimMapKey(operand)] = value;
          }
        }
        break;
      }
      case HloOpcode::kDot: {
        const HloInstruction* lhs = ins->operand(0);
        const HloInstruction* rhs = ins->operand(1);
        const auto& dot_dnums = ins->dot_dimension_numbers();
        int64_t space_base_dim = dot_dnums.lhs_batch_dimensions_size();
        const auto& lhs_batch_dims =
            ins->dot_dimension_numbers().lhs_batch_dimensions();
        const auto& rhs_batch_dims =
            ins->dot_dimension_numbers().rhs_batch_dimensions();
        tsl::protobuf::RepeatedField<int64_t> lhs_space_dims, rhs_space_dims;
        std::tie(lhs_space_dims, rhs_space_dims) =
            GetSpaceDims(lhs->shape(), rhs->shape(), dot_dnums);

        if (batch_map.contains(GetBatchDimMapKey(ins))) {
          int value = batch_map[GetBatchDimMapKey(ins)];
          if (!batch_map.contains(GetBatchDimMapKey(lhs))) {
            for (int i = 0; i < lhs_batch_dims.size(); ++i) {
              if (value == i) {
                batch_map[GetBatchDimMapKey(lhs)] = lhs_batch_dims[i];
                break;
              }
            }
            if (value == space_base_dim) {
              batch_map[GetBatchDimMapKey(lhs)] = lhs_space_dims[0];
            }
          }

          if (!batch_map.contains(GetBatchDimMapKey(rhs))) {
            for (int i = 0; i < rhs_batch_dims.size(); ++i) {
              if (value == i) {
                batch_map[GetBatchDimMapKey(rhs)] = rhs_batch_dims[i];
                break;
              }
            }
            if (value == space_base_dim + 1) {
              batch_map[GetBatchDimMapKey(rhs)] = rhs_space_dims[0];
            }
          }
        }

        break;
      }
      case HloOpcode::kConvolution: {
        const HloInstruction* lhs = ins->operand(0);
        const HloInstruction* rhs = ins->operand(1);
        const auto& conv_dnums = ins->convolution_dimension_numbers();

        if (batch_map.contains(GetBatchDimMapKey(ins))) {
          int value = batch_map[GetBatchDimMapKey(ins)];
          if (value == conv_dnums.output_batch_dimension() &&
              !batch_map.contains(GetBatchDimMapKey(lhs))) {
            batch_map[GetBatchDimMapKey(lhs)] =
                conv_dnums.input_batch_dimension();
          }

          if (value == conv_dnums.output_feature_dimension() &&
              !batch_map.contains(GetBatchDimMapKey(rhs))) {
            batch_map[GetBatchDimMapKey(rhs)] =
                conv_dnums.kernel_output_feature_dimension();
          }
        }

        break;
      }
      case HloOpcode::kGetTupleElement: {
        const HloInstruction* op = ins->operand(0);
        if (batch_map.contains(GetBatchDimMapKey(ins, ins->tuple_index()))) {
          batch_map[GetBatchDimMapKey(op)] =
              batch_map[GetBatchDimMapKey(ins, ins->tuple_index())];
        }
        break;
      }
      case HloOpcode::kTuple:
        for (size_t i = 0; i < ins->operand_count(); ++i) {
          const HloInstruction* op = ins->operand(i);
          if (batch_map.contains(GetBatchDimMapKey(ins, i))) {
            batch_map[GetBatchDimMapKey(op)] =
                batch_map[GetBatchDimMapKey(ins, i)];
          }
        }
        break;
      case HloOpcode::kWhile: {
        const HloInstruction* op = ins->operand(0);
        for (size_t i = 0; i < op->shape().tuple_shapes_size(); ++i) {
          if (batch_map.contains(GetBatchDimMapKey(ins, i))) {
            batch_map[GetBatchDimMapKey(op, i)] =
                batch_map[GetBatchDimMapKey(ins, i)];
            batch_map[GetBatchDimMapKey(ins->while_body()->root_instruction(),
                                        i)] =
                batch_map[GetBatchDimMapKey(ins, i)];
            batch_map[GetBatchDimMapKey(
                ins->while_body()->parameter_instruction(0), i)] =
                batch_map[GetBatchDimMapKey(ins, i)];
            batch_map[GetBatchDimMapKey(
                ins->while_condition()->parameter_instruction(0), i)] =
                batch_map[GetBatchDimMapKey(ins, i)];
          }
        }
        break;
      }
      case HloOpcode::kCustomCall:
        break;
      default:
        break;
    }
  }
}

// This function was unable to thoroughly propagate batch dim to all
// instructions. It only propagates to 14 other instructions in the 8b model.
// Batch dimension analysis that finds the batch dimension of each instruction.
InstructionBatchDimMap BuildInstructionBatchDimMap(
    const HloInstructionSequence& sequence) {
  InstructionBatchDimMap batch_map;
  const std::vector<HloInstruction*>& instructions = sequence.instructions();

  // We use the first dot or convolution as the source to start batch dim
  // propagation. Assume the first dim of the first dot is the batch dim.
  int batch_dim_of_source = 0;

  // Find the source of batch_dim propagation
  bool set_the_next_dot_conv = true;
  for (const HloInstruction* ins : instructions) {
    if (ins->opcode() == HloOpcode::kDot ||
        ins->opcode() == HloOpcode::kConvolution) {
      if (set_the_next_dot_conv) {
        set_the_next_dot_conv = false;
        batch_map[ins->name()] = batch_dim_of_source;
      }
    }

    if (ins->IsCustomCall(kPipelineMarker) &&
        absl::StrContains(ins->metadata().op_type(), "start")) {
      // Reset the status after meet a new pipeline marker.
      set_the_next_dot_conv = true;
    }
  }
  int64_t previous_cnt = 0;
  while (true) {
    // Forward propagation: propagate from operand
    BatchDimMapForward(instructions, batch_map);
    // Backward propagation: propagate to operands
    BatchDimMapBackward(instructions, batch_map);
    LOG(INFO) << "batch_map size:  " << batch_map.size();
    if (batch_map.size() == previous_cnt) {
      break;
    }
    previous_cnt = batch_map.size();
  }
  return batch_map;
}

// Returns true if there is one row with only infinity cost.
bool AllInfinityCosts(
    const std::vector<std::vector<double>>& resharding_costs) {
  for (const auto& costs : resharding_costs) {
    bool all_infinity = true;
    if (costs.empty()) {
      all_infinity = false;
      continue;
    }
    for (const auto& cost : costs) {
      if (cost < kInfinityCost) {
        all_infinity = false;
      }
    }
    if (all_infinity) {
      return true;
    }
  }
  return false;
}

// Remove duplicated strategies with the same output sharding spec.
// If duplicates strategies have different costs, an arbitrary one will be
// chosen. A exception is replicated strategy. Only *real* replicated strategies
// are preserved, which are generated with name "R" or starting with "R
// (allreduce". Unintended replicated strategies are removed, which are ones
// that were not intended to be replicated when being generating, but ending up
// being replicated, which could happen when, for example, generating 2D
// sharding for a 1D mesh shape.
void RemoveDuplicatedStrategy(std::unique_ptr<StrategyGroup>& strategy_group) {
  if (strategy_group->is_tuple) {
    for (auto& child : strategy_group->childs) {
      RemoveDuplicatedStrategy(child);
    }
  } else if (!strategy_group->following) {
    if (strategy_group->strategies.empty()) return;
    std::vector<ShardingStrategy> new_vector;
    std::vector<ShardingStrategy> deduped_replicated_strategies;
    absl::flat_hash_set<std::string> added;
    size_t num_skipped_due_to_infinity_costs = 0;
    for (size_t i = 0; i < strategy_group->strategies.size(); ++i) {
      if (AllInfinityCosts(
              strategy_group->strategies[i].communication_resharding_costs)) {
        num_skipped_due_to_infinity_costs++;
        continue;
      }
      std::string key =
          strategy_group->strategies[i].output_sharding.ToString();
      if (!strategy_group->strategies[i].input_shardings.empty()) {
        for (const auto& sharding :
             strategy_group->strategies[i].input_shardings) {
          key += "/" + (sharding.has_value() ? sharding->ToString() : "none");
        }
      }
      if (!added.contains(key)) {
        added.insert(key);
        if (!strategy_group->strategies[i].output_sharding.IsReplicated()) {
          new_vector.push_back(std::move(strategy_group->strategies[i]));
        } else {
          deduped_replicated_strategies.push_back(
              std::move(strategy_group->strategies[i]));
        }
      }
    }
    CHECK_LT(num_skipped_due_to_infinity_costs,
             strategy_group->strategies.size())
        << "All strategies removed due to infinite resharding costs";
    // Keeps replicated strategies as the last ones.
    if (!deduped_replicated_strategies.empty()) {
      for (size_t i = 0; i < deduped_replicated_strategies.size(); ++i) {
        new_vector.push_back(std::move(deduped_replicated_strategies[i]));
      }
    }
    strategy_group->strategies = std::move(new_vector);
  }
}

bool IsDivisible(const HloInstruction* ins, const Array<int64_t>& device_mesh,
                 absl::Span<const int64_t> tensor_dims,
                 absl::Span<const int64_t> mesh_dims) {
  CHECK_EQ(tensor_dims.size(), mesh_dims.size());
  for (int64_t i = 0; i < tensor_dims.size(); ++i) {
    if (ins->shape().dimensions(tensor_dims[i]) %
            device_mesh.dim(mesh_dims[i]) !=
        0) {
      return false;
    }
  }
  return true;
}

// Set sharding, and apply transpose if necessary.
void SetSharding(HloInstruction* to_split, const HloSharding& output_spec,
                 const HloInstruction* ref_inst,
                 const HloInstruction* shape_inst,
                 StableHashSet<const HloInstruction*>& modified) {
  modified.insert(to_split);
  if (DimensionsEqual(to_split->shape(), ref_inst->shape())) {
    to_split->set_sharding(output_spec);
  } else {
    CHECK(shape_inst != nullptr);
    CHECK_EQ(shape_inst->opcode(), HloOpcode::kTranspose);
    to_split->set_sharding(hlo_sharding_util::TransposeSharding(
        output_spec, shape_inst->dimensions()));
  }
}

bool IsAlwaysReplicated(const HloInstruction* inst) {
  if (inst->opcode() == HloOpcode::kConstant) {
    return true;
  }
  if (inst->shape().rank() == 0) {
    return true;
  }
  if (inst->opcode() == HloOpcode::kBroadcast) {
    return IsAlwaysReplicated(inst->operand(0));
  }
  return false;
}

// Try to reduce the boundary set to its common ancestor
void TryReduceWithCommonAncestor(StableHashSet<HloInstruction*>& replicated_set,
                                 StableHashSet<HloInstruction*>& boundary_set,
                                 StableHashSet<HloInstruction*>& consumer_set,
                                 const AliasMap& alias_map) {
  if (boundary_set.size() != 2) {
    return;
  }

  HloInstruction* ancestor = nullptr;
  StableHashSet<HloInstruction*> path;
  for (HloInstruction* node : boundary_set) {
    HloInstruction* cur = node;
    while (cur->operand_count() == 1) {
      HloInstruction* operand =
          PassThroughCustomCallMarkerOperand(cur->mutable_operand(0), cur);
      if (replicated_set.contains(operand)) {
        path.insert(cur);
      }
      cur = operand;
    }

    if (ancestor == nullptr) {
      ancestor = cur;
    } else {
      if (ancestor != cur) {
        // The nodes in boundary set do not have a common ancestor.
        // This reduction fails.
        return;
      }
    }
  }
  if (ancestor == nullptr) {
    return;
  }

  // Find a common ancestor, reduce the boundary set
  boundary_set.clear();
  boundary_set.insert(ancestor);
  for (auto x : path) {
    replicated_set.erase(x);
  }
  consumer_set.insert(ancestor);
}

void UseAllReduceForGradAcc(StableHashSet<HloInstruction*>& replicated_set,
                            const HloInstruction* inst) {
  if (inst->users().size() != 1) {
    return;
  }

  // Find the add instruction for grad accumulation, skip the identity marker
  // for remat and other elementwise ops.
  const HloInstruction* add =
      PassThroughCustomCallMarkerUser(inst->users().front(), inst);
  if (add->opcode() == HloOpcode::kGetTupleElement ||
      add->opcode() == HloOpcode::kTranspose) {
    if (add->users().size() != 1) {
      return;
    }
    add = add->users().front();
  }

  if (add->opcode() == HloOpcode::kAdd) {
    // Skip multiple adds introduced by AllReduceReassociate.
    while (add->users().size() == 1 &&
           add->users().front()->opcode() == HloOpcode::kAdd) {
      add = add->users().front();
    }
    CHECK_EQ(add->users().size(), 1);
    // Skip the end marker of backward computation
    add = PassThroughCustomCallMarkerUser(add->users().front(), add);

    // Do not partition the dot, add and parameter, so we can generate
    // all-reduce for grad accumulation.
    std::function<void(const HloInstruction*)> dfs_remove;
    dfs_remove = [&](const HloInstruction* cur) {
      if (!replicated_set.contains(cur)) {
        return;
      }

      replicated_set.erase(cur);
      for (auto x : cur->operands()) {
        dfs_remove(PassThroughCustomCallMarkerOperand(x, cur));
      }
    };

    dfs_remove(add);
  }
}

// Gets values in |array| along |dim| while keeping indices at other
// dimensions at 0, e.g., array is 2D and dim = 1, this returns array[0, 1],
// array[1, 1], array [2, 1], ....
// Returns error status if dim >= array.num_dimensions().
absl::StatusOr<std::vector<int64_t>> GetValuesAlongOneDim(
    const Array<int64_t>& array, int dim) {
  if (dim >= array.num_dimensions()) {
    return absl::OutOfRangeError(absl::StrCat(
        "Input dim (", dim,
        ") should be smaller than the number of dimensions of array (",
        array.num_dimensions(), ")."));
  }
  std::vector<int64_t> ret;
  std::vector<int64_t> indices(array.num_dimensions(), 0);
  for (int i = 0; i < array.dim(dim); ++i) {
    indices[dim] = i;
    ret.push_back(array(indices));
  }
  return ret;
}

// Check whether a sequence is an arithmetic sequence.
absl::StatusOr<int64_t> CheckArithmeticSequence(
    absl::Span<const int64_t> sequence) {
  if (sequence.size() < 2) {
    return absl::OutOfRangeError(
        "Invalid device id assignment: sequence.size() < 2");
  }
  int64_t delta = sequence[1] - sequence[0];
  for (int i = 2; i < sequence.size(); ++i) {
    if (sequence[i] - sequence[i - 1] != delta) {
      return absl::OutOfRangeError(
          "Invalid device id assignment: sequence[i] - sequence[i - 1] != "
          "delta");
    }
  }
  return delta;
}

bool IsValidTileAssignment(const HloSharding& spec) {
  if (IsUndefined(spec)) {
    return false;
  }

  if (spec.IsReplicated()) {
    return true;
  }

  // Check all tile dims
  const auto& tile_assignment = spec.tile_assignment();
  for (int i = 0; i < tile_assignment.num_dimensions(); i++) {
    if (tile_assignment.dim(i) != 1) {
      std::vector<int64_t> device_ids =
          GetValuesAlongOneDim(tile_assignment.array(), i).value();
      auto status_or_delta = CheckArithmeticSequence(device_ids);
      if (!status_or_delta.ok()) {
        return false;
      }
    }
  }

  return true;
}

int64_t NumTileDimensions(const HloSharding& spec) {
  if (spec.IsReplicated()) {
    return -1;
  }
  int64_t num_tile_dims = 0;
  for (int i = 0; i < spec.tile_assignment().num_dimensions(); i++) {
    if (spec.tile_assignment().dim(i) != 1) {
      num_tile_dims++;
    }
  }
  return num_tile_dims;
}

bool TileAssignmentMatchesMesh(const HloSharding& spec,
                               const Array<int64_t>& mesh) {
  int sharded_dims = 0;
  for (int i = 0; i < spec.tile_assignment().num_dimensions(); ++i) {
    if (spec.tile_assignment().dim(i) > 1) {
      sharded_dims++;
    }
  }
  for (int i = 0; i < mesh.num_dimensions(); ++i) {
    if (mesh.dim(i) > 1) {
      sharded_dims--;
    }
  }
  return sharded_dims <= 0;
}

absl::StatusOr<std::vector<int64_t>> GetTensorDimToMeshDimNoCrash(
    int64_t tensor_shape_rank, const HloSharding& spec,
    const Array<int64_t>& device_mesh, bool consider_reverse_device_meshes) {
  if (spec.IsReplicated()) {
    return std::vector<int64_t>(tensor_shape_rank, -1);
  }
  // Check the compatibility of tensor_shape_rank and spec
  if (tensor_shape_rank != spec.TiledDataRank()) {
    return absl::InvalidArgumentError(
        "Tensor shape rank should be equal to the tiled data rank of the input "
        "spec.");
  }

  auto check_mesh =
      [&](const Array<int64_t>& mesh) -> std::optional<std::vector<int64_t>> {
    // Permute the dimensions (or axes in numpy term), find the transform that
    // makes tile_assignment == device_mesh.
    std::vector<int64_t> axes(mesh.num_dimensions());
    absl::c_iota(axes, 0);
    bool found = false;
    do {
      Array<int64_t> transposed_mesh = Transpose(mesh, axes);
      if (std::equal(transposed_mesh.begin(), transposed_mesh.end(),
                     spec.tile_assignment().array().begin())) {
        found = true;
        break;
      }
    } while (absl::c_next_permutation(axes));
    if (found) {
      return std::optional<std::vector<int64_t>>(axes);
    } else {
      return std::nullopt;
    }
  };

  // This is an expensive search, as we try all possible meshes obtained by
  // reversing a subset of the mesh axes. Reversed shardings only occur due to
  // the somewhat rare kReverse HLO op. The hope therefore is that most calls to
  // the function that reach here will find a mapping within the first iteration
  // of the loop below.
  bool found = false;
  std::vector<int64_t> axes(device_mesh.num_dimensions());
  size_t num_subsets =
      consider_reverse_device_meshes ? (1 << device_mesh.num_dimensions()) : 1;
  std::vector<int64_t> reverse_dimensions;
  for (size_t i = 0; i < num_subsets; ++i) {
    reverse_dimensions.clear();
    for (size_t j = 0; j < device_mesh.num_dimensions(); ++j) {
      if (i & (1 << j)) {
        reverse_dimensions.push_back(j);
      }
    }
    Array<int64_t> new_mesh(device_mesh.dimensions());
    new_mesh.Each([&](absl::Span<const int64_t> indices, int64_t* device) {
      std::vector<int64_t> original_indices(indices.begin(), indices.end());
      for (int64_t d : reverse_dimensions) {
        original_indices[d] = new_mesh.dim(d) - 1 - original_indices[d];
      }
      *device = device_mesh(original_indices);
    });
    if (auto result = check_mesh(new_mesh); result.has_value()) {
      axes = result.value();
      found = true;
      break;
    }
  }

  if (!found) {
    return absl::NotFoundError(
        absl::StrCat("Could not find mapping for ", spec.ToString(),
                     " with device mesh ", device_mesh.ToString()));
  }

  if (!TileAssignmentMatchesMesh(spec, device_mesh)) {
    return absl::InvalidArgumentError(
        "Device mesh and tile assignment need to have the same number of "
        "sharded dims.");
  }

  // Transform tile_assignment_dimensions using found transformation (axes).
  std::vector<int64_t> tensor_dim_to_device_dim(tensor_shape_rank, -1);
  int mesh_index = 0;
  for (int i = 0; i < tensor_shape_rank; ++i) {
    if (spec.tile_assignment().dim(i) != 1) {
      while (device_mesh.dim(axes[mesh_index]) == 1) {
        mesh_index++;
      }
      tensor_dim_to_device_dim[i] = axes[mesh_index];
      mesh_index++;
    }
  }
  return tensor_dim_to_device_dim;
}

std::vector<int64_t> GetTensorDimToMeshDim(
    int64_t tensor_shape_rank, const HloSharding& spec,
    const Array<int64_t>& device_mesh, bool consider_reverse_device_meshes) {
  auto mapping_or = GetTensorDimToMeshDimNoCrash(
      tensor_shape_rank, spec, device_mesh, consider_reverse_device_meshes);
  if (mapping_or.ok()) {
    return mapping_or.value();
  } else {
    LOG(FATAL) << mapping_or.status().message();
  }
}

absl::StatusOr<Shape> ComputeIntermediateShape(
    const HloSharding& src_sharding, const HloSharding& dst_sharding,
    const Shape& shape, const Array<int64_t>& device_mesh) {
  int64_t src_n_dim = NumTileDimensions(src_sharding);

  const HloSharding* sharding_1d;

  if (src_n_dim == 1) {
    sharding_1d = &src_sharding;
  } else {
    sharding_1d = &dst_sharding;
  }

  // Find an intermediate shape
  std::vector<int64_t> inter_shape_dims;

  for (size_t i = 0; i < shape.rank(); ++i) {
    if (sharding_1d->tile_assignment().dim(i) == 1) {
      inter_shape_dims.push_back(shape.dimensions(i));
    } else {
      // TODO(b/333750146): Support this case instead of bailing here
      if (shape.dimensions(i) % device_mesh.dim(0) != 0) {
        return absl::InternalError("Indivisible tensor dims");
      }
      inter_shape_dims.push_back(device_mesh.dim(0));
      inter_shape_dims.push_back(shape.dimensions(i) / device_mesh.dim(0));
    }
  }
  VLOG(3) << " SHAPE " << static_cast<int>(shape.element_type()) << " "
          << spmd::ToString(inter_shape_dims) << " " << src_sharding.ToString()
          << "\n"
          << dst_sharding.ToString();
  return ShapeUtil::MakeShape(shape.element_type(), inter_shape_dims);
}

HloInstruction* ReshardTensor(HloInstruction* tensor,
                              const HloSharding& src_sharding,
                              const HloSharding& dst_sharding,
                              const Array<int64_t>& device_mesh) {
  const Shape& shape = tensor->shape();
  HloComputation* computation = tensor->parent();

  int64_t src_n_dim = NumTileDimensions(src_sharding);
  int64_t dst_n_dim = NumTileDimensions(dst_sharding);

  HloInstruction* replace_with = nullptr;
  if (src_n_dim != dst_n_dim && src_n_dim != -1 && dst_n_dim != -1) {
    absl::StatusOr<Shape> inter_shape = ComputeIntermediateShape(
        src_sharding, dst_sharding, shape, device_mesh);
    if (inter_shape.ok()) {
      std::optional<HloSharding> src_inter_sharding =
          hlo_sharding_util::ReshapeSharding(shape, *inter_shape, src_sharding);
      std::optional<HloSharding> dst_inter_sharding =
          hlo_sharding_util::ReshapeSharding(shape, *inter_shape, dst_sharding);
      if (!src_inter_sharding.has_value() || !dst_inter_sharding.has_value()) {
        src_inter_sharding = HloSharding::Replicate();
        dst_inter_sharding = HloSharding::Replicate();
        LOG(WARNING) << "Invalid mixed mesh shape resharding.";
      }

      HloInstruction* src_inter = computation->AddInstruction(
          HloInstruction::CreateReshape(*inter_shape, tensor));
      src_inter->set_sharding(*src_inter_sharding);

      HloInstruction* dst_inter = computation->AddInstruction(
          HloInstruction::CreateReshape(*inter_shape, src_inter));
      dst_inter->set_sharding(*dst_inter_sharding);

      replace_with = computation->AddInstruction(
          HloInstruction::CreateReshape(shape, dst_inter));
    } else {
      replace_with = computation->AddInstruction(
          HloInstruction::CreateReshape(shape, tensor));
    }
  } else {
    replace_with = computation->AddInstruction(
        HloInstruction::CreateReshape(shape, tensor));
  }
  replace_with->set_sharding(dst_sharding);

  return replace_with;
}

absl::Status FixMixedMeshShapeReshardingGetTupleElementWithTupleOutput(
    HloInstruction* inst,
    const std::vector<std::optional<HloSharding>>& dst_shardings,
    const Array<int64_t>& device_mesh) {
  size_t tuple_size = inst->shape().tuple_shapes_size();
  const HloSharding& current_sharding = inst->sharding();

  bool need_to_reshard = false;
  for (size_t i = 0; i < tuple_size; ++i) {
    CHECK(!inst->shape().tuple_shapes(i).IsTuple());
    const HloSharding& element_current_sharding =
        current_sharding.GetSubSharding(inst->shape(),
                                        {static_cast<int64_t>(i)});
    std::optional<HloSharding> element_dst_sharding_opt = dst_shardings[i];

    // Extract tuple element
    if (element_dst_sharding_opt.has_value() &&
        element_current_sharding != *element_dst_sharding_opt) {
      need_to_reshard = true;
    }
  }

  if (!need_to_reshard) {
    return absl::OkStatus();
  }

  const PtrVec<HloInstruction*>& inst_users = inst->users();
  std::vector<HloInstruction*> resharded;
  std::vector<HloSharding> reassembled_tuple_shardings;
  resharded.reserve(tuple_size);
  reassembled_tuple_shardings.reserve(tuple_size);
  for (size_t i = 0; i < tuple_size; ++i) {
    const HloSharding& element_current_sharding =
        current_sharding.GetSubSharding(inst->shape(),
                                        {static_cast<int64_t>(i)});
    std::optional<HloSharding> element_dst_sharding_opt = dst_shardings[i];

    // Extract tuple element
    HloInstruction* element =
        inst->parent()->AddInstruction(HloInstruction::CreateGetTupleElement(
            inst->shape().tuple_shapes(i), inst, i));
    if (!element_dst_sharding_opt.has_value() ||
        element_current_sharding == *element_dst_sharding_opt) {
      resharded.push_back(std::move(element));
      reassembled_tuple_shardings.push_back(element_current_sharding);
    } else {
      HloInstruction* replace_with =
          ReshardTensor(element, element_current_sharding,
                        *element_dst_sharding_opt, device_mesh);
      resharded.push_back(std::move(replace_with));
      reassembled_tuple_shardings.push_back(*element_dst_sharding_opt);
    }
  }

  HloInstruction* reassembled_tuple =
      inst->parent()->AddInstruction(HloInstruction::CreateTuple(resharded));
  reassembled_tuple->set_sharding(
      HloSharding::Tuple(inst->shape(), reassembled_tuple_shardings));

  for (HloInstruction* user : inst_users) {
    TF_RETURN_IF_ERROR(inst->ReplaceUseWith(user, reassembled_tuple));
  }
  return absl::OkStatus();
}

absl::Status FixMixedMeshShapeReshardingGetTupleElement(
    HloInstruction* inst, const HloSharding& dst_sharding,
    const Array<int64_t>& device_mesh,
    absl::flat_hash_map<std::string, std::vector<HloSharding>>&
        preserve_shardings) {
  const HloInstruction* operand = inst->operand(0);
  const HloSharding& input_tuple_sharding = operand->sharding();
  size_t index = inst->tuple_index();
  if (input_tuple_sharding.tuple_elements()[index] == dst_sharding) {
    return absl::OkStatus();
  }

  // Make a copy of the users before things are modified.
  const PtrVec<HloInstruction*> inst_users = inst->users();

  const HloSharding& src_sharding =
      input_tuple_sharding.tuple_elements()[index];
  CHECK(operand->shape().IsTuple());

  HloInstruction* replace_with =
      ReshardTensor(inst, src_sharding, dst_sharding, device_mesh);
  inst->set_sharding(src_sharding);
  size_t size = ByteSizeOfShape(replace_with->shape()) / (1024 * 1024 * 1024);
  if (size > 1) {
    LOG(WARNING) << "Large reshape instruction inserted (operand of "
                 << inst->name() << ") with size " << size
                 << "GB: " << replace_with->ToString();
  }

  for (HloInstruction* user : inst_users) {
    TF_RETURN_IF_ERROR(inst->ReplaceUseWith(user, replace_with));
  }

  auto iter = preserve_shardings.find(inst->name());
  if (iter != preserve_shardings.end()) {
    preserve_shardings[replace_with->name()] =
        std::vector<HloSharding>(iter->second);
    preserve_shardings.erase(inst->name());
  }
  return absl::OkStatus();
}

absl::Status FixMixedMeshShapeResharding(HloInstruction* inst, int operand_num,
                                         const HloSharding& dst_sharding,
                                         const Array<int64_t>& device_mesh,
                                         ReshardingCache* resharding_cache) {
  HloInstruction* operand = inst->mutable_operand(operand_num);
  if (operand->opcode() == HloOpcode::kOutfeed ||
      operand->opcode() == HloOpcode::kSendDone) {
    return absl::OkStatus();
  }

  CHECK(operand->has_sharding()) << inst->name() << " " << operand->name();
  if (operand->sharding() == dst_sharding) {
    return absl::OkStatus();
  }

  if (operand->shape().IsToken()) {
    // This is the token operand for outfeed. We directly set the dst_sharding
    // for the operand in this case, as it doesn't make sense to reshard a
    // token.
    CHECK_EQ(operand_num, 1);
    operand->set_sharding(dst_sharding);
  } else {
    const HloSharding& src_sharding = operand->sharding();
    HloInstruction* replace_with = nullptr;
    // Query cache first
    std::vector<std::pair<HloSharding, HloInstruction*>>* cache_vector =
        nullptr;
    if (resharding_cache != nullptr) {
      cache_vector = &((*resharding_cache)[operand]);
      for (const std::pair<HloSharding, HloInstruction*>& entry :
           *cache_vector) {
        if (entry.first == dst_sharding) {
          replace_with = entry.second;
        }
      }
    }

    if (replace_with != nullptr) {
      // Do nothing
    } else {
      replace_with =
          ReshardTensor(operand, src_sharding, dst_sharding, device_mesh);
      if (cache_vector != nullptr) {
        cache_vector->push_back({dst_sharding, replace_with});
      }
    }

    size_t size = ByteSizeOfShape(replace_with->shape()) / (1024 * 1024 * 1024);
    if (size > 1) {
      LOG(WARNING) << "Large reshape instruction inserted (operand of "
                   << inst->name() << ") with size " << size
                   << "GB: " << replace_with->ToString();
    }

    TF_RETURN_IF_ERROR(inst->ReplaceOperandWith(operand_num, replace_with));
  }
  return absl::OkStatus();
}

bool IsParameterConvert(const HloInstruction* inst) {
  if (inst->opcode() == HloOpcode::kConvert &&
      inst->operand(0)->opcode() == HloOpcode::kParameter) {
    return true;
  }
  return false;
}

bool AllUsersAreReduce(const HloInstruction* inst) {
  for (const HloInstruction* user : inst->users()) {
    if (user->opcode() != HloOpcode::kReduce) {
      return false;
    }
  }
  return true;
}

std::vector<int64_t> GetDimensionMapping(
    const absl::Span<const int64_t> reduced_dimensions,
    const int64_t op_count) {
  std::vector<int64_t> mapping;
  mapping.reserve(op_count);
  int64_t dim_to_counter = 0;
  for (int64_t op_dim = 0; op_dim < op_count; ++op_dim) {
    if (absl::c_find(reduced_dimensions, op_dim) != reduced_dimensions.end()) {
      // If op_dim is in reduce_dimensions, it means this op_dim is reduced
      // (gone) in output dimensions.
      mapping.push_back(-1);
    } else {
      // Otherwise create the mapping in order.
      mapping.push_back(dim_to_counter);
      dim_to_counter += 1;
    }
  }
  return mapping;
}

bool IsDivisible(int64_t numerator, int64_t denominator) {
  return (numerator % denominator == 0);
}

std::vector<std::vector<int64_t>> GetReplicaGroupsAlongOneDimension(
    const Array<int64_t>& device_mesh, int32_t communication_dim) {
  CHECK_LT(communication_dim, device_mesh.num_dimensions());
  std::vector<int64_t> indices(device_mesh.num_dimensions(), 0);
  std::vector<std::vector<int64_t>> replica_groups;
  device_mesh.Each([&](absl::Span<const int64_t> indices, int64_t device) {
    std::vector<int64_t> group;
    group.reserve(device_mesh.dim(communication_dim));
    if (indices[communication_dim] != 0) {
      return;
    }
    for (size_t i = 0; i < device_mesh.dim(communication_dim); ++i) {
      std::vector<int64_t> mutable_indices(indices.begin(), indices.end());
      mutable_indices[communication_dim] = i;
      group.push_back(device_mesh(mutable_indices));
    }
    replica_groups.push_back(std::move(group));
  });
  return replica_groups;
}

// Create a HloSharding that tiles some tensor dims on some device mesh dims.
HloSharding Tile(const Shape& tensor_shape,
                 absl::Span<const int64_t> tensor_dims,
                 absl::Span<const int64_t> mesh_dims,
                 const Array<int64_t>& device_mesh) {
  CHECK_EQ(tensor_dims.size(), mesh_dims.size());
  CHECK(tensor_shape.IsArray());
  std::vector<int64_t> tile_assignment_dimensions(tensor_shape.rank(), 1);

  // Split on certain mesh dimensions
  int64_t split_prod = 1;
  for (size_t i = 0; i < tensor_dims.size(); ++i) {
    tile_assignment_dimensions[tensor_dims[i]] = device_mesh.dim(mesh_dims[i]);
    split_prod *= device_mesh.dim(mesh_dims[i]);
  }
  // Replicate on remaining mesh dimensions
  bool replicate_on_last_tile_dim = false;
  if (split_prod < device_mesh.num_elements()) {
    tile_assignment_dimensions.push_back(device_mesh.num_elements() /
                                         split_prod);
    replicate_on_last_tile_dim = true;
  }

  // Map device ids from device_mesh to tile_assignment_devices
  std::vector<int64_t> tile_assignment_devices;
  tile_assignment_devices.reserve(device_mesh.num_elements());

  std::vector<int64_t> tmp_indices(device_mesh.num_dimensions(), 0);
  std::function<void(int64_t, std::vector<int64_t>)>
      generate_tile_assignment_devices;
  generate_tile_assignment_devices = [&](int64_t tensor_dim,
                                         std::vector<int64_t> mesh_indices) {
    if (tensor_dim == tensor_shape.rank() - 1) {
      AppendFlattenElements(&tile_assignment_devices, device_mesh, mesh_indices,
                            -1, tmp_indices);
    } else {
      int64_t next_tensor_dim = tensor_dim + 1;
      int64_t next_mesh_dim = -1;

      int64_t index = GetIndex(tensor_dims, next_tensor_dim);
      if (index >= 0) {
        next_mesh_dim = mesh_dims[index];
      }

      for (int64_t i = 0; i < tile_assignment_dimensions[next_tensor_dim];
           ++i) {
        if (next_mesh_dim != -1) {
          mesh_indices[next_mesh_dim] = i;
        }
        generate_tile_assignment_devices(next_tensor_dim, mesh_indices);
      }
    }
  };

  std::vector<int64_t> mesh_indices(device_mesh.num_dimensions(), -1);
  generate_tile_assignment_devices(-1, mesh_indices);

  // Make HloSharding
  Array<int64_t> tile_assignment(tile_assignment_dimensions);
  VLOG(9) << "shape: " << tensor_shape.ToString();
  VLOG(9) << "tensor dims: " << ToString(tensor_dims);
  VLOG(9) << "mesh dims: " << ToString(mesh_dims);
  VLOG(9) << "tile_assignment: " << ToString(tile_assignment.dimensions());
  tile_assignment.SetValues(tile_assignment_devices);

  return replicate_on_last_tile_dim
             ? HloSharding::PartialTile(std::move(tile_assignment))
             : HloSharding::Tile(std::move(tile_assignment));
}

AliasMap BuildAliasMap(const HloModule* module,
                       const HloInputOutputAliasConfig& alias_config) {
  AliasMap alias_map;

  HloComputation* entry = module->entry_computation();
  const auto& parameter_instructions = entry->parameter_instructions();
  const HloInstruction* output_tuple = entry->root_instruction();

  if (IsCustomCallMarker(output_tuple)) {
    output_tuple = output_tuple->operand(0);
  }

  absl::flat_hash_map<int64_t, absl::flat_hash_map<int64_t, HloInstruction*>>
      parameter_index_to_operand_map;
  alias_config.ForEachAlias([&](const ShapeIndex& output_index,
                                const HloInputOutputAliasConfig::Alias& alias) {
    CHECK_LT(alias.parameter_index.size(), 2)
        << "Do not support alias parameter index that is larger than 1D: "
        << alias.ToString();
    CHECK_EQ(output_index.size(), 1)
        << "Do not support alias with output_index that is larger than 1D: "
        << output_index.ToString();
    if (!alias.parameter_index.empty()) {
      for (size_t i = 0;
           i < parameter_instructions[alias.parameter_number]->users().size();
           ++i) {
        auto user = parameter_instructions[alias.parameter_number]->users()[i];
        if (user->opcode() == HloOpcode::kGetTupleElement) {
          parameter_index_to_operand_map[alias.parameter_number]
                                        [user->tuple_index()] = user;
        }
      }
    }
  });

  alias_config.ForEachAlias([&](const ShapeIndex& output_index,
                                const HloInputOutputAliasConfig::Alias& alias) {
    // We skip some checks here as they have been performed above already.
    const HloInstruction* dst_ins = output_tuple->operand(output_index.front());
    HloInstruction* src_ins = nullptr;
    if (alias.parameter_index.empty()) {
      src_ins = parameter_instructions[alias.parameter_number];
    } else {
      // alias.parameter_index.size() == 1 per the CHECK_LT statement.
      auto outer_iter =
          parameter_index_to_operand_map.find(alias.parameter_number);
      if (outer_iter != parameter_index_to_operand_map.end()) {
        auto tuple_index_to_operand_map = outer_iter->second;
        auto inner_iter =
            tuple_index_to_operand_map.find(alias.parameter_index.front());
        if (inner_iter != tuple_index_to_operand_map.end()) {
          src_ins = inner_iter->second;
        }
      }
    }
    if (src_ins != nullptr) {
      alias_map[dst_ins] = src_ins;
    }
  });

  return alias_map;
}

AliasSet BuildAliasSet(const HloModule* module,
                       const HloInputOutputAliasConfig& alias_config,
                       const StrategyMap& strategy_map) {
  // We also look at alias_config to adjust the edge cost for aliases (donated
  // buffer). Typically, old weights and new weights are aliases, so we should
  // let them have the same sharding spec.
  HloComputation* entry = module->entry_computation();
  const auto& parameter_instructions = entry->parameter_instructions();
  const HloInstruction* output_tuple = entry->root_instruction();

  AliasSet alias_set;
  std::function<void(const StrategyGroup*, const StrategyGroup*)>
      traverse_tuple_alias;
  traverse_tuple_alias = [&](const StrategyGroup* src_strategy_group,
                             const StrategyGroup* dst_strategy_group) {
    if (src_strategy_group->is_tuple) {
      CHECK(dst_strategy_group->is_tuple);
      CHECK_EQ(src_strategy_group->childs.size(),
               dst_strategy_group->childs.size());
      for (size_t i = 0; i < src_strategy_group->childs.size(); ++i) {
        traverse_tuple_alias(src_strategy_group->childs[i].get(),
                             dst_strategy_group->childs[i].get());
      }
    } else {
      alias_set.insert(
          {src_strategy_group->node_idx, dst_strategy_group->node_idx});
    }
  };
  alias_config.ForEachAlias([&](const ShapeIndex& output_index,
                                const HloInputOutputAliasConfig::Alias& alias) {
    CHECK_LT(alias.parameter_index.size(), 2)
        << "Do not support alias parameter index that is larger than 1D: "
        << alias.ToString();
    CHECK_EQ(output_index.size(), 1)
        << "Do not support alias with output_index that is larger than 1D: "
        << output_index.ToString();

    HloInstruction* param_ins = parameter_instructions[alias.parameter_number];
    if (alias.parameter_index.empty()) {
      traverse_tuple_alias(
          strategy_map.at(param_ins).get(),
          strategy_map.at(output_tuple)->childs[output_index.front()].get());
    } else {
      // parameter_instructions[alias.parameter_number] is a tuple.
      // alias.parameter_index.size() == 1 per the CHECK_LT statement.
      traverse_tuple_alias(
          strategy_map.at(param_ins)
              ->childs[alias.parameter_index.front()]
              .get(),
          strategy_map.at(output_tuple)->childs[output_index.front()].get());
    }
  });

  // Uses the same sharding spec for while loop and conditional related
  // instructions.
  for (const HloComputation* computation : module->computations()) {
    for (const HloInstruction* instruction : computation->instructions()) {
      if (instruction->opcode() == HloOpcode::kWhile) {
        // Aliasing between the while op, and the parameters of its body and
        // conditional computations is handled by making the latter follow the
        // input tuple to thew while loop in the function
        // BuildStrategyAndCost().
        traverse_tuple_alias(
            strategy_map.at(instruction).get(),
            strategy_map.at(instruction->while_body()->root_instruction())
                .get());
      } else if (instruction->opcode() == HloOpcode::kConditional) {
        auto branch_computations = instruction->branch_computations();
        for (size_t i = 0; i < branch_computations.size(); ++i) {
          const auto& branch_computation = branch_computations[i];
          traverse_tuple_alias(
              strategy_map.at(instruction).get(),
              strategy_map.at(branch_computation->root_instruction()).get());
          traverse_tuple_alias(
              strategy_map.at(instruction->operand(i + 1)).get(),
              strategy_map.at(branch_computation->parameter_instruction(0))
                  .get());
        }
      }
    }
  }
  return alias_set;
}

absl::Status CheckAliasSetCompatibility(const AliasSet& alias_set,
                                        const StrategyGroups& strategy_groups,
                                        const HloInstructionSequence& sequence,
                                        bool crash_on_error) {
  const std::vector<HloInstruction*>& instructions = sequence.instructions();
  // Checks the compatibility
  for (const auto& pair : alias_set) {
    const StrategyGroup* src_strategy_group = strategy_groups[pair.first];
    const StrategyGroup* dst_strategy_group = strategy_groups[pair.second];

    size_t compatible_cnt = 0;
    bool replicated = false;
    for (size_t i = 0; i < src_strategy_group->strategies.size(); ++i) {
      for (size_t j = 0; j < dst_strategy_group->strategies.size(); ++j) {
        if (src_strategy_group->strategies[i].output_sharding ==
            dst_strategy_group->strategies[j].output_sharding) {
          compatible_cnt += 1;
          if (src_strategy_group->strategies[i]
                  .output_sharding.IsReplicated()) {
            replicated = true;
          }
        }
      }
    }

    if (compatible_cnt == 1 &&
        (replicated && (src_strategy_group->strategies.size() > 1 ||
                        dst_strategy_group->strategies.size() > 1))) {
      LOG(WARNING)
          << "Alias pair has only replicated strategy in common. This "
             "will result in choosing replicated strategy for these "
             "tensors and may result in large memory consumption: "
          << "(" << instructions.at(src_strategy_group->instruction_id)->name()
          << ", " << instructions.at(dst_strategy_group->instruction_id)->name()
          << ")" << "\n"
          << "(" << src_strategy_group->node_idx << ", "
          << dst_strategy_group->node_idx << ")\n"
          << src_strategy_group->ToString() << "\n"
          << dst_strategy_group->ToString();
    }
    if (compatible_cnt <= 0) {
      std::string err_msg = absl::StrCat(
          "Alias pair does not have any sharding strategy in common: (",
          instructions.at(src_strategy_group->instruction_id)->name(), ", ",
          instructions.at(dst_strategy_group->instruction_id)->name(), ")\n(",
          src_strategy_group->node_idx, ", ", dst_strategy_group->node_idx,
          ")\n", src_strategy_group->ToString(), "\n",
          dst_strategy_group->ToString());
      if (crash_on_error) {
        LOG(FATAL) << err_msg;
      } else {
        LOG(WARNING) << err_msg;
        return absl::InternalError(err_msg);
      }
    }
  }
  return absl::OkStatus();
}

size_t VectorGreaterThanOneElementCount(absl::Span<const int64_t> span,
                                        bool omit_last_dim) {
  return VectorGreaterThanOneElementIndices(span, omit_last_dim).size();
}

std::vector<int64_t> VectorGreaterThanOneElementIndices(
    absl::Span<const int64_t> span, bool omit_last_dim) {
  std::vector<int64_t> result;
  for (size_t i = 0; i < span.size(); i++) {
    if (i == span.size() - 1 && omit_last_dim) {
      continue;
    }
    if (span.at(i) > 1) {
      result.push_back(i);
    }
  }
  return result;
}

// Given a sharding, and a shape index, obtains the subsharding corresponding to
// that shape index. This function works whether or not the provided sharding is
// a tuple, unlike HloSharding::GetSubSharding.
HloSharding GetSubSharding(const HloSharding& sharding,
                           const Shape& original_tuple_shape,
                           const ShapeIndex& index) {
  return sharding.IsTuple()
             ? sharding.GetSubSharding(original_tuple_shape, index)
             : sharding;
}

int64_t ByteSizeOfShapeWithSharding(const Shape& original_shape,
                                    std::optional<HloSharding> sharding) {
  int64_t total_size = 0;
  auto add_to_total_size = [&total_size](const Shape& shape) {
    total_size +=
        ShapeUtil::ByteSizeOf(shape, /*pointer_size=*/kAutoShardingPointerSize);
  };
  ShapeUtil::ForEachSubshape(original_shape, [&total_size, &add_to_total_size,
                                              &sharding, &original_shape](
                                                 const Shape& subshape,
                                                 const ShapeIndex& index) {
    if (subshape.IsTuple()) {
      add_to_total_size(subshape);
    } else if (subshape.IsArray() && sharding.has_value()) {
      add_to_total_size(
          GetSubSharding(*sharding, original_shape, index).TileShape(subshape));
    } else if (subshape.IsArray()) {
      add_to_total_size(subshape);
    } else if (subshape.IsToken()) {
      // Tokens are considered to have a size of 0
    } else {
      total_size += kAutoShardingPointerSize;
    }
  });
  return total_size;
}

int64_t ByteSizeOfShapeIfShardedAcrossDevices(
    const Shape& shape, int64_t num_devices,
    std::optional<HloSharding> sharding) {
  if (sharding.has_value()) {
    return ByteSizeOfShapeWithSharding(shape, sharding);
  }

  int64_t total_size = 0;
  ShapeUtil::ForEachSubshape(
      shape, [&total_size, &num_devices](const Shape& subshape,
                                         const ShapeIndex& index) {
        if (subshape.IsTuple()) {
          total_size += ShapeUtil::ByteSizeOf(
              subshape, /*pointer_size=*/kAutoShardingPointerSize);
          return;
        }
        int64_t byte_size = ByteSizeOfShape(subshape);
        absl::Span<const int64_t> subshape_dims = subshape.dimensions();
        auto max_dim_it = absl::c_max_element(subshape_dims);
        if (max_dim_it != subshape_dims.end() && *max_dim_it >= num_devices) {
          byte_size /= num_devices;
        }
        total_size += byte_size;
      });

  return total_size;
}

HloInstruction* FindInstruction(
    const std::vector<HloInstruction*>& instructions, absl::string_view name) {
  auto it = absl::c_find_if(
      instructions, [&](HloInstruction* i) { return i->name() == name; });
  if (it != instructions.end()) {
    return *it;
  }
  return nullptr;
}

absl::StatusOr<std::optional<HloSharding>>
AdjustShardingWithPartialMeshShapePerElement(
    const HloSharding& sharding,
    const absl::flat_hash_set<int64_t>& valid_shards, int64_t total_num_devices,
    bool crash_on_error) {
  if (sharding.TotalNumTiles() > total_num_devices &&
      VectorGreaterThanOneElementCount(
          sharding.tile_assignment().dimensions()) > valid_shards.size()) {
    for (auto shard : valid_shards) {
      bool contains_shard = false;
      for (auto dim : sharding.tile_assignment().dimensions()) {
        if (dim == shard) {
          contains_shard = true;
          break;
        }
      }

      if (!contains_shard && !sharding.IsReplicated()) {
        auto err_msg = absl::StrCat(
            "There is a mismatch between the user provided sharding ",
            sharding.ToString(),
            " and the device mesh. This case is currently unsupported.");
        if (crash_on_error) {
          LOG(FATAL) << err_msg;
        } else {
          LOG(WARNING) << err_msg;
          return absl::InternalError(err_msg);
        }
      }
    }

    std::vector<int64_t> new_tile_assignment_dimensions;
    if (sharding.ReplicateOnLastTileDim()) {
      // If replicate on valid_shards dimensions, turns this instruction
      // into replicate.
      // If two mesh dimensions are the same size, it becomes replicated too.
      if (valid_shards.find(sharding.tile_assignment().dim(
              sharding.tile_assignment().num_dimensions() - 1)) !=
          valid_shards.end()) {
        return HloSharding::Replicate();
      }
      // If replicate on other dimensions, remove the
      // replicate_on_last_tile
      new_tile_assignment_dimensions.assign(
          sharding.tile_assignment().dimensions().begin(),
          sharding.tile_assignment().dimensions().end());
      new_tile_assignment_dimensions.erase(
          new_tile_assignment_dimensions.end() - 1);
    } else {
      new_tile_assignment_dimensions.assign(
          sharding.tile_assignment().dimensions().begin(),
          sharding.tile_assignment().dimensions().end());
      absl::flat_hash_set<int64_t> current_shards;
      for (const auto dim : new_tile_assignment_dimensions) {
        if (dim > 1) {
          current_shards.insert(dim);
        }
      }
      if (current_shards.size() == 1) {
        // Two mesh dimensions are the same size. Keep the first sharded
        // dimension.
        for (int32_t i = new_tile_assignment_dimensions.size() - 1; i >= 0;
             i--) {
          if (new_tile_assignment_dimensions[i] > 1 &&
              valid_shards.find(new_tile_assignment_dimensions[i]) !=
                  valid_shards.end()) {
            new_tile_assignment_dimensions[i] = 1;
            break;
          }
        }
      } else {
        for (size_t i = 0; i < new_tile_assignment_dimensions.size(); i++) {
          if (new_tile_assignment_dimensions[i] > 1 &&
              valid_shards.find(new_tile_assignment_dimensions[i]) ==
                  valid_shards.end()) {
            new_tile_assignment_dimensions[i] = 1;
          }
        }
      }
    }
    Array<int64_t> tile_assignment(new_tile_assignment_dimensions);
    std::vector<int64_t> device_ids = std::vector<int64_t>(total_num_devices);
    // Set arbitrary values because it will not be used.
    std::iota(device_ids.begin(), device_ids.end(), 0);
    tile_assignment.SetValues(device_ids);
    return HloSharding::Tile(std::move(tile_assignment));
  }
  return std::nullopt;
}

absl::StatusOr<bool> AdjustShardingsWithPartialMeshShape(
    const std::vector<HloInstruction*>& instructions,
    const absl::flat_hash_set<const HloInstruction*>& instructions_to_shard,
    const std::vector<int64_t>& mesh_shape, int64_t total_num_devices,
    bool crash_on_error) {
  bool changed = false;
  absl::flat_hash_set<int64_t> valid_shards;
  for (const auto shape : mesh_shape) {
    if (shape > 1) {
      valid_shards.insert(shape);
    }
  }
  for (HloInstruction* inst : instructions) {
    if (!inst->has_sharding() || !instructions_to_shard.contains(inst)) {
      continue;
    }
    if (inst->shape().IsTuple()) {
      ShapeTree<HloSharding> output_tuple_sharding(inst->shape(), Undefined());
      std::vector<HloSharding> output_flattened_shardings;
      for (size_t i = 0; i < inst->shape().tuple_shapes_size(); i++) {
        auto shape = inst->shape().tuple_shapes(i);
        auto sharding = inst->sharding().tuple_elements()[i];
        if (sharding.IsUnknown()) {
          output_flattened_shardings.push_back(sharding);
          continue;
        }
        absl::StatusOr<std::optional<HloSharding>> new_sharding_result =
            AdjustShardingWithPartialMeshShapePerElement(
                sharding, valid_shards, total_num_devices, crash_on_error);
        if (new_sharding_result.ok()) {
          if (new_sharding_result.value().has_value()) {
            output_flattened_shardings.push_back(*new_sharding_result.value());
          } else {
            output_flattened_shardings.push_back(sharding);
          }
        } else {
          return new_sharding_result.status();
        }
      }
      size_t i = 0;
      for (auto& leaf : output_tuple_sharding.leaves()) {
        leaf.second = output_flattened_shardings[i++];
      }
      inst->set_sharding(HloSharding::Tuple(output_tuple_sharding));
    } else {
      absl::StatusOr<std::optional<HloSharding>> sharding_result =
          AdjustShardingWithPartialMeshShapePerElement(
              inst->sharding(), valid_shards, total_num_devices,
              crash_on_error);
      if (sharding_result.ok()) {
        if (sharding_result.value().has_value()) {
          inst->set_sharding(*sharding_result.value());
          changed = true;
        }
      } else {
        return sharding_result.status();
      }
    }
  }
  return changed;
}

std::vector<std::vector<int64_t>> DecomposeMeshShapes(
    std::vector<int64_t> mesh_shape) {
  // Get the ranking order based on the size of each value.
  std::vector<int64_t> ranking_order;
  std::vector<std::vector<int64_t>> partial_mesh_shapes;
  std::vector<std::pair<int64_t, size_t>> pairs(mesh_shape.size());
  for (size_t i = 0; i < mesh_shape.size(); i++) {
    pairs[i] = {mesh_shape[i], i};
  }
  // For vector of size 3, the sorted indices happen to be the same as their
  // rankings. mesh_shapes over 3 elements are not supported by AutoSharding.
  std::sort(pairs.begin(), pairs.end(),
            std::greater<std::pair<int64_t, size_t>>());

  std::vector<int64_t> partial_mesh_shape(mesh_shape.size(), 1);
  // Starts from the largest dimension of mesh_shape.
  for (size_t i = 0; i < pairs.size(); i++) {
    if (pairs[i].first == 1) {
      break;
    }
    partial_mesh_shape[pairs[i].second] = pairs[i].first;
    // Needs to copy partial_mesh_shape.
    partial_mesh_shapes.push_back(partial_mesh_shape);
  }
  return partial_mesh_shapes;
}

bool OutputInputSameShapes(const HloInstruction* ins) {
  for (auto op : ins->operands()) {
    if (ins->shape() != op->shape()) {
      return false;
    }
  }
  return true;
}

bool IsEntryComputationInputOrOutput(const HloModule* module,
                                     const HloInstruction* ins) {
  for (const auto param :
       module->entry_computation()->parameter_instructions()) {
    if (param->name() == ins->name()) {
      return true;
    }
  }
  if (module->entry_computation()->root_instruction() == ins) {
    return true;
  }
  return false;
}

void ComputeInstructionExecutionCountsHelper(
    const HloComputation* computation, int64_t computation_execution_count,
    int64_t static_loop_iteration_count_estimate,
    absl::flat_hash_map<const HloInstruction*, int64_t>&
        instruction_execution_counts) {
  for (const HloInstruction* instruction : computation->instructions()) {
    (instruction_execution_counts)[instruction] = computation_execution_count;
    if (instruction->opcode() == HloOpcode::kWhile) {
      int64_t loop_iteration_count = static_loop_iteration_count_estimate;
      if (std::optional<int64_t> upper_bound =
              ComputeWhileLoopTripCountUpperBound(instruction)) {
        loop_iteration_count = *upper_bound;
      }
      int64_t while_body_condition_execution_count =
          computation_execution_count * loop_iteration_count;
      ComputeInstructionExecutionCountsHelper(
          instruction->while_body(),
          /* computation_execution_count */
          while_body_condition_execution_count,
          /* loop_iteration_count_estimate */
          static_loop_iteration_count_estimate, instruction_execution_counts);
      ComputeInstructionExecutionCountsHelper(
          instruction->while_condition(),
          /* computation_execution_count */
          while_body_condition_execution_count,
          /* loop_iteration_count_estimate */
          static_loop_iteration_count_estimate, instruction_execution_counts);
    } else if (instruction->opcode() == HloOpcode::kConditional) {
      // TODO(pratikf): For now, we do not scale down the execution counts of
      // branch statements, though we should at some point.
      PtrVec<HloComputation*> branch_computations =
          instruction->branch_computations();
      for (size_t i = 0; i < branch_computations.size(); ++i) {
        ComputeInstructionExecutionCountsHelper(
            branch_computations[i],
            /* computation_execution_count */
            computation_execution_count,
            /* loop_iteration_count_estimate */
            static_loop_iteration_count_estimate, instruction_execution_counts);
      }
    }
  }
}

absl::flat_hash_map<const HloInstruction*, int64_t>
ComputeInstructionExecutionCounts(
    const HloModule* module, int64_t static_loop_iteration_count_estimate) {
  absl::flat_hash_map<const HloInstruction*, int64_t>
      instruction_execution_counts;
  ComputeInstructionExecutionCountsHelper(module->entry_computation(), 1,
                                          static_loop_iteration_count_estimate,
                                          instruction_execution_counts);
  return instruction_execution_counts;
}

void EnumerateAllPossibleMeshShapesHelper(
    int64_t num_devices, size_t num_mesh_dims,
    std::vector<int64_t> current_shape,
    std::vector<std::vector<int64_t>>& all_shapes) {
  if (current_shape.size() == num_mesh_dims - 1) {
    current_shape.push_back(num_devices);
    if (spmd::VectorGreaterThanOneElementCount(current_shape) <= 2) {
      all_shapes.push_back(current_shape);
    }
    return;
  } else {
    int64_t current_dim = 1;
    while (current_dim <= num_devices) {
      std::vector<int64_t> new_shape(current_shape);
      new_shape.push_back(current_dim);
      EnumerateAllPossibleMeshShapesHelper(
          num_devices / current_dim, num_mesh_dims, new_shape, all_shapes);
      current_dim *= 2;
    }
  }
}

std::vector<std::vector<int64_t>> InferMeshShapesToTry(
    const HloModule& module) {
  int64_t sharding_1d = -1;
  absl::flat_hash_set<std::vector<int64_t>> shardings_nd;
  std::function<void(const HloSharding&)> process_sharding;
  process_sharding = [&sharding_1d, &shardings_nd,
                      &process_sharding](const HloSharding& sharding) {
    if (sharding.IsTuple()) {
      for (const HloSharding& child : sharding.tuple_elements()) {
        process_sharding(child);
      }
    } else if (!sharding.IsReplicated() && !sharding.IsTileMaximal() &&
               !sharding.IsManual()) {
      absl::Span<const int64_t> dims = sharding.tile_assignment().dimensions();
      std::vector<int64_t> dims_greater_than_one;
      for (const int64_t dim : dims) {
        if (dim > 1) {
          dims_greater_than_one.push_back(dim);
        }
      }
      if (dims_greater_than_one.size() == 1) {
        CHECK(sharding_1d == -1 || sharding_1d == dims_greater_than_one[0]);
        sharding_1d = dims_greater_than_one[0];
      } else {
        std::sort(dims_greater_than_one.begin(), dims_greater_than_one.end());
        shardings_nd.insert(dims_greater_than_one);
      }
    }
  };

  for (const HloComputation* comp : module.computations()) {
    for (const HloInstruction* ins : comp->instructions()) {
      if (ins->has_sharding()) {
        process_sharding(ins->sharding());
      }
    }
  }

  if (shardings_nd.empty() && sharding_1d < 0) {
    return {};
  } else if (shardings_nd.empty()) {
    CHECK_GE(sharding_1d, 0);
    return {{1, sharding_1d}};
  } else {
    std::vector<std::vector<int64_t>> result;
    for (std::vector<int64_t> mesh : shardings_nd) {
      do {
        result.push_back(std::vector<int64_t>(mesh));
      } while (std::next_permutation(std::begin(mesh), std::end(mesh)));
    }
    return result;
  }
}

std::vector<std::vector<int64_t>> InferOrEnumerateMeshShapesToTry(
    const HloModule& module, int64_t num_devices, int num_mesh_dims,
    bool symmetrical_mesh_dims) {
  std::vector<std::vector<int64_t>> mesh_shapes = InferMeshShapesToTry(module);
  if (mesh_shapes.empty()) {
    EnumerateAllPossibleMeshShapesHelper(num_devices, num_mesh_dims, {},
                                         mesh_shapes);
  }
  if (symmetrical_mesh_dims) {
    absl::flat_hash_set<absl::btree_multiset<int64_t>> dedup_result;
    for (const std::vector<int64_t>& mesh_shape : mesh_shapes) {
      dedup_result.insert(
          absl::btree_multiset<int64_t>(mesh_shape.begin(), mesh_shape.end()));
    }

    mesh_shapes.clear();

    for (const absl::btree_multiset<int64_t>& mesh_shape_set : dedup_result) {
      mesh_shapes.push_back(
          std::vector<int64_t>(mesh_shape_set.begin(), mesh_shape_set.end()));
    }
  }

  return mesh_shapes;
}

bool IsShardingMisaligned(const HloSharding& sharding, const Shape& shape) {
  if (shape.IsTuple()) {
    for (size_t i = 0; i < shape.tuple_shapes_size(); ++i) {
      if (IsShardingMisaligned(
              sharding.IsTuple()
                  ? sharding.GetSubSharding(shape, {static_cast<int64_t>(i)})
                  : sharding,
              shape.tuple_shapes(i))) {
        return true;
      }
    }
    return false;
  }

  if (sharding.IsReplicated() || sharding.IsManual() || sharding.IsUnknown() ||
      sharding.IsTileMaximal()) {
    return false;
  }

  for (size_t i = 0; i < shape.rank(); ++i) {
    int64_t shape_dim = shape.dimensions()[i];
    int64_t sharding_dim = sharding.tile_assignment().dim(i);
    if (shape_dim % sharding_dim != 0) {
      return true;
    }
  }
  return false;
}

HloSharding ReplaceGivenShardingsWithUnknownForTuple(
    const HloSharding& sharding, const Shape& shape,
    absl::Span<const bool> to_replace_sharding_ids) {
  std::vector<HloSharding> new_tuple_shardings;
  int64_t num_elements = sharding.tuple_elements().size();
  for (int32_t i = 0; i < num_elements; ++i) {
    bool can_change_sharding = to_replace_sharding_ids.size() == 1
                                   ? to_replace_sharding_ids[0]
                                   : to_replace_sharding_ids[i];
    if (can_change_sharding) {
      new_tuple_shardings.push_back(HloSharding::Unknown());
    } else {
      new_tuple_shardings.push_back(sharding.tuple_elements()[i]);
    }
  }

  return HloSharding::Tuple(shape, new_tuple_shardings);
}

absl::StatusOr<int64_t> GetPartialReduceReductionDim(
    const HloInstruction* ins) {
  constexpr char kReductionDimKey[] = "reduction_dim";
  if (ins->raw_backend_config_string().empty()) {
    return absl::InternalError(
        "No backend config for a PartialReduce custom call.");
  }
  Json::Value parsed_json;
  Json::Reader json_reader;
  json_reader.parse(ins->raw_backend_config_string(), parsed_json,
                    /* collectComments */ false);
  if (!parsed_json.isObject()) {
    return absl::InternalError(
        "Error when parsing json backend config for a PartialReduce custom "
        "call.");
  }
  if (!parsed_json.isMember(kReductionDimKey)) {
    return absl::InternalError(
        "No backend config found for a PartialReduce custom call.");
  }

  if (!parsed_json[kReductionDimKey].isInt64()) {
    return absl::InternalError(
        "Error when extracting the reduction key from the json backend config "
        "of a PartialReduce custom call.");
  }

  return parsed_json[kReductionDimKey].asInt64();
}

bool OpEncountersShardToFull(const HloInstruction* op) {
  std::queue<const HloInstruction*> queue;
  queue.push(op);

  absl::flat_hash_set<const HloInstruction*> visited;
  while (!queue.empty()) {
    const HloInstruction* instruction = queue.front();
    queue.pop();
    if (visited.contains(instruction)) {
      continue;
    }
    visited.insert(instruction);

    for (const HloComputation* computation :
         instruction->called_computations()) {
      for (const HloInstruction* parameter :
           computation->parameter_instructions()) {
        if (spmd::IsSPMDShardToFullShapeCustomCall(parameter)) {
          return true;
        } else if (spmd::IsSPMDFullToShardShapeCustomCall(parameter) ||
                   parameter == instruction || visited.contains(parameter)) {
          continue;
        }
        queue.push(parameter);
      }
    }

    for (const HloInstruction* user : instruction->users()) {
      if (spmd::IsSPMDShardToFullShapeCustomCall(user)) {
        return true;
      } else if (spmd::IsSPMDFullToShardShapeCustomCall(user) ||
                 visited.contains(user)) {
        continue;
      }
      queue.push(user);
    }
  }

  return false;
}

}  // namespace spmd
}  // namespace xla

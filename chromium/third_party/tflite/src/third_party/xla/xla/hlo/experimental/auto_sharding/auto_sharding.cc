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

#include "xla/hlo/experimental/auto_sharding/auto_sharding.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <limits>
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
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "xla/array.h"
#include "xla/hlo/experimental/auto_sharding/auto_sharding_cost_graph.h"
#include "xla/hlo/experimental/auto_sharding/auto_sharding_memory.h"
#include "xla/hlo/experimental/auto_sharding/auto_sharding_option.h"
#include "xla/hlo/experimental/auto_sharding/auto_sharding_solver.h"
#include "xla/hlo/experimental/auto_sharding/auto_sharding_strategy.h"
#include "xla/hlo/experimental/auto_sharding/auto_sharding_util.h"
#include "xla/hlo/experimental/auto_sharding/auto_sharding_wrapper.h"
#include "xla/hlo/experimental/auto_sharding/cluster_environment.h"
#include "xla/hlo/experimental/auto_sharding/matrix.h"
#include "xla/hlo/experimental/auto_sharding/metrics.h"
#include "xla/hlo/experimental/auto_sharding/profiling_result.h"
#include "xla/hlo/ir/hlo_casting_utils.h"
#include "xla/hlo/ir/hlo_input_output_alias_config.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/hlo/ir/hlo_module.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/hlo/ir/hlo_schedule.h"
#include "xla/hlo/ir/hlo_sharding.h"
#include "xla/hlo/transforms/hlo_constant_splitter.h"
#include "xla/hlo/utils/hlo_live_range.h"
#include "xla/hlo/utils/hlo_sharding_util.h"
#include "xla/service/buffer_value.h"
#include "xla/service/call_graph.h"
#include "xla/service/computation_layout.h"
#include "xla/service/dump.h"
#include "xla/service/hlo_alias_analysis.h"
#include "xla/service/hlo_buffer.h"
#include "xla/service/hlo_cost_analysis.h"
#include "xla/service/hlo_dce.h"
#include "xla/service/hlo_memory_scheduler.h"
#include "xla/service/hlo_value.h"
#include "xla/service/optimize_input_output_buffer_alias.h"
#include "xla/service/sharding_propagation.h"
#include "xla/shape.h"
#include "xla/shape_tree.h"
#include "xla/shape_util.h"
#include "xla/util.h"
#include "xla/xla_data.pb.h"
#include "tsl/platform/errors.h"
#include "tsl/platform/status.h"
#include "tsl/platform/statusor.h"

namespace xla {
namespace spmd {

namespace {
constexpr double kSaltiplier = 0.0;  // This value (0.0) disables salting.
}  // namespace

// Compute the resharding cost vector from multiple possible strategies to a
// desired sharding spec.
std::vector<double> CommunicationReshardingCostVector(
    const StrategyGroup* strategy_group, const Shape& operand_shape,
    const HloSharding& required_sharding,
    const ClusterEnvironment& cluster_env) {
  CHECK(!strategy_group->is_tuple) << "Only works with strategy vector.";
  std::vector<double> ret;
  ret.reserve(strategy_group->strategies.size());
  auto required_sharding_for_resharding = required_sharding.IsTileMaximal()
                                              ? HloSharding::Replicate()
                                              : required_sharding;
  for (const auto& x : strategy_group->strategies) {
    ret.push_back(cluster_env.ReshardingCost(operand_shape, x.output_sharding,
                                             required_sharding_for_resharding));
  }
  return ret;
}

double ComputeMemoryReshardingCost(const Shape& shape,
                                   const HloSharding& src_sharding,
                                   const HloSharding& dst_sharding,
                                   const Array<int64_t>& device_mesh) {
  int64_t src_n_dim = NumTileDimensions(src_sharding);
  int64_t dst_n_dim = NumTileDimensions(dst_sharding);

  int64_t src_sharded_bytes = ByteSizeOfShapeWithSharding(shape, src_sharding);
  double result = std::max(src_sharded_bytes,
                           ByteSizeOfShapeWithSharding(shape, dst_sharding));

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
      }

      result = std::max(
          result,
          static_cast<double>(std::max(
              ByteSizeOfShapeWithSharding(*inter_shape, src_inter_sharding),
              ByteSizeOfShapeWithSharding(*inter_shape, dst_inter_sharding))));
    }
  }
  return result - src_sharded_bytes;
}

std::vector<double> MemoryReshardingCostVector(
    const StrategyGroup* strategy_group, const Shape& operand_shape,
    const HloSharding& required_sharding,
    const ClusterEnvironment& cluster_env) {
  CHECK(!strategy_group->is_tuple) << "Only works with strategy vector.";
  std::vector<double> ret;
  ret.reserve(strategy_group->strategies.size());
  auto required_sharding_for_resharding = required_sharding.IsTileMaximal()
                                              ? HloSharding::Replicate()
                                              : required_sharding;
  CHECK_OK(required_sharding.Validate(operand_shape))
      << strategy_group->ToString();
  for (const auto& x : strategy_group->strategies) {
    ret.push_back(ComputeMemoryReshardingCost(operand_shape, x.output_sharding,
                                              required_sharding_for_resharding,
                                              cluster_env.device_mesh_));
  }
  return ret;
}

// Factory functions for StrategyGroup.
std::unique_ptr<StrategyGroup> CreateLeafStrategyGroupWithoutInNodes(
    const size_t instruction_id, StrategyGroups& strategy_groups) {
  auto strategy_group = std::make_unique<StrategyGroup>();
  strategy_group->is_tuple = false;
  strategy_group->node_idx = strategy_groups.size();
  strategy_groups.push_back(strategy_group.get());
  strategy_group->instruction_id = instruction_id;
  return strategy_group;
}

// Factory functions for StrategyGroup.
std::unique_ptr<StrategyGroup> CreateLeafStrategyGroup(
    const size_t instruction_id, const HloInstruction* ins,
    const StrategyMap& strategy_map, StrategyGroups& strategy_groups) {
  auto strategy_group =
      CreateLeafStrategyGroupWithoutInNodes(instruction_id, strategy_groups);
  for (int64_t i = 0; i < ins->operand_count(); ++i) {
    strategy_group->in_nodes.push_back(strategy_map.at(ins->operand(i)).get());
  }
  return strategy_group;
}

std::unique_ptr<StrategyGroup> CreateTupleStrategyGroup(
    const size_t instruction_id) {
  auto strategy_group = std::make_unique<StrategyGroup>();
  strategy_group->is_tuple = true;
  strategy_group->node_idx = -1;
  strategy_group->instruction_id = instruction_id;
  return strategy_group;
}

// Compute the resharding costs as well as input shardings (when missing) for
// all operands of a given instruction, and an output sharding for that
// instruction.
std::pair<ReshardingCosts, ReshardingCosts>
GenerateReshardingCostsAndMissingShardingsForAllOperands(
    const HloInstruction* ins, const HloSharding& output_sharding,
    const StrategyMap& strategy_map, const ClusterEnvironment& cluster_env,
    const CallGraph& call_graph,
    std::vector<std::optional<HloSharding>>& input_shardings) {
  ReshardingCosts communication_resharding_costs;
  ReshardingCosts memory_resharding_costs;
  if (input_shardings.empty() && ins->operand_count() > 0) {
    input_shardings.resize(ins->operand_count());
  }
  for (int64_t k = 0; k < ins->operand_count(); ++k) {
    auto operand = ins->operand(k);
    if (operand->shape().IsToken() || operand->shape().rank() == 0) {
      communication_resharding_costs.push_back(std::vector<double>(
          strategy_map.at(operand)->strategies.size(), 0.0));
      memory_resharding_costs.push_back(std::vector<double>(
          strategy_map.at(operand)->strategies.size(), 0.0));
      if (!input_shardings[k].has_value()) {
        input_shardings[k] = HloSharding::Replicate();
      }
    } else {
      std::optional<HloSharding> cur_input_sharding;
      CHECK_EQ(input_shardings.size(), ins->operand_count());
      if (input_shardings[k].has_value()) {
        cur_input_sharding = input_shardings[k];
      } else {
        cur_input_sharding = GetInputSharding(
            ins, k, output_sharding, call_graph, cluster_env.NumDevices());
      }
      bool is_sharding_default_replicated = false;
      if (!cur_input_sharding.has_value()) {
        if ((ins->opcode() == HloOpcode::kGather && k == 0) ||
            (ins->opcode() == HloOpcode::kScatter && k != 0)) {
          is_sharding_default_replicated = true;
          cur_input_sharding = HloSharding::Replicate();
        } else if (ins->opcode() == HloOpcode::kCustomCall) {
          is_sharding_default_replicated = true;
          cur_input_sharding = HloSharding::Replicate();
        } else if (ins->opcode() == HloOpcode::kRngBitGenerator) {
          cur_input_sharding = HloSharding::Replicate();
        }
      }
      CHECK(cur_input_sharding.has_value());
      if (!input_shardings[k].has_value()) {
        input_shardings[k] = cur_input_sharding;
      }
      auto operand_strategies = strategy_map.at(operand).get();
      auto operand_shape = operand->shape();
      if (ins->opcode() == HloOpcode::kGather && k == 0 &&
          is_sharding_default_replicated) {
        VLOG(2) << "Zeroing out operand 0 resharding costs for gather sharding "
                << output_sharding.ToString();
        communication_resharding_costs.push_back(
            std::vector<double>(operand_strategies->strategies.size(), 0));
        memory_resharding_costs.push_back(
            std::vector<double>(operand_strategies->strategies.size(), 0));
        input_shardings[k] = std::nullopt;
      } else {
        communication_resharding_costs.push_back(
            CommunicationReshardingCostVector(
                operand_strategies, ins->operand(k)->shape(),
                *cur_input_sharding, cluster_env));
        memory_resharding_costs.push_back(MemoryReshardingCostVector(
            operand_strategies, ins->operand(k)->shape(), *cur_input_sharding,
            cluster_env));
      }
    }
  }
  return std::make_pair(communication_resharding_costs,
                        memory_resharding_costs);
}

std::tuple<ReshardingCosts, ReshardingCosts,
           std::vector<std::optional<HloSharding>>>
GenerateReshardingCostsAndShardingsForAllOperands(
    const HloInstruction* ins, const HloSharding& output_sharding,
    const StrategyMap& strategy_map, const ClusterEnvironment& cluster_env,
    const CallGraph& call_graph) {
  std::vector<std::optional<HloSharding>> input_shardings_optional;
  std::pair<ReshardingCosts, ReshardingCosts> resharding_costs =
      GenerateReshardingCostsAndMissingShardingsForAllOperands(
          ins, output_sharding, strategy_map, cluster_env, call_graph,
          input_shardings_optional);
  for (const auto& sharding_optional : input_shardings_optional) {
    CHECK(sharding_optional.has_value());
  }

  return {resharding_costs.first, resharding_costs.second,
          input_shardings_optional};
}

// When computing resharding costs for inputs, this function assumes that the
// shape of the input is the same as the shape of the output (i.e., the `shape`
// operand to the function).
void FollowArrayOrTokenStrategyGroup(
    const StrategyGroup& src_strategy_group, const Shape& shape,
    const size_t instruction_id, const ClusterEnvironment& cluster_env,
    const StableHashMap<NodeIdx, std::vector<ShardingStrategy>>&
        pretrimmed_strategy_map,
    StrategyGroup& strategy_group) {
  CHECK(shape.IsArray() || shape.IsToken());

  std::vector<ShardingStrategy> pretrimmed_strategies;
  // Only follows the given strategy when there is no other strategy to be
  // restored.
  auto pretrimmed_strategy_map_it =
      pretrimmed_strategy_map.find(src_strategy_group.node_idx);
  if (pretrimmed_strategy_map_it != pretrimmed_strategy_map.end()) {
    pretrimmed_strategies = pretrimmed_strategy_map_it->second;
  } else {
    strategy_group.following = &src_strategy_group;
  }

  strategy_group.strategies.reserve(src_strategy_group.strategies.size());
  // Creates the sharding strategies and restores trimmed strategies, if any.
  for (int64_t sid = 0; sid < src_strategy_group.strategies.size() +
                                  pretrimmed_strategies.size();
       ++sid) {
    const HloSharding* output_spec;
    if (sid < src_strategy_group.strategies.size()) {
      output_spec = &src_strategy_group.strategies[sid].output_sharding;
    } else {
      output_spec =
          &pretrimmed_strategies[sid - src_strategy_group.strategies.size()]
               .output_sharding;
      VLOG(1) << "Adding outspec from the trimmed strategy map: "
              << output_spec->ToString();
    }
    const std::string name = ToStringSimple(*output_spec);
    double compute_cost = 0, communication_cost = 0;
    double memory_cost = ByteSizeOfShapeWithSharding(shape, *output_spec);
    size_t num_in_nodes = strategy_group.in_nodes.size();
    std::vector<std::optional<HloSharding>> input_shardings(num_in_nodes,
                                                            *output_spec);
    ReshardingCosts communication_resharding_costs;
    ReshardingCosts memory_resharding_costs;
    for (size_t i = 0; i < strategy_group.in_nodes.size(); ++i) {
      communication_resharding_costs.push_back(
          CommunicationReshardingCostVector(strategy_group.in_nodes[i], shape,
                                            *output_spec, cluster_env));
      memory_resharding_costs.push_back(MemoryReshardingCostVector(
          strategy_group.in_nodes[i], shape, *output_spec, cluster_env));
    }

    strategy_group.strategies.push_back(
        ShardingStrategy({name, *output_spec, compute_cost, communication_cost,
                          memory_cost, communication_resharding_costs,
                          memory_resharding_costs, input_shardings}));
  }
}

std::unique_ptr<StrategyGroup> HandlePartialReduce(
    const HloInstruction* ins, const size_t instruction_id,
    StrategyGroups& strategy_groups, const ClusterEnvironment& cluster_env,
    StrategyMap& strategy_map, const CallGraph& call_graph) {
  absl::StatusOr<int64_t> reduction_dim = GetPartialReduceReductionDim(ins);
  CHECK_OK(reduction_dim);
  const Shape& shape = ins->shape();
  const HloInstruction* operand = ins->operand(0);
  const StrategyGroup* src_strategy_group = strategy_map.at(operand).get();

  std::unique_ptr<StrategyGroup> strategy_group =
      CreateTupleStrategyGroup(instruction_id);
  int64_t output_size = shape.tuple_shapes_size();
  for (size_t i = 0; i < output_size; ++i) {
    std::unique_ptr<StrategyGroup> child_strategy_group =
        CreateLeafStrategyGroupWithoutInNodes(instruction_id, strategy_groups);
    child_strategy_group->in_nodes.push_back(src_strategy_group);
    child_strategy_group->following = src_strategy_group;
    for (int64_t sid = 0; sid < src_strategy_group->strategies.size(); ++sid) {
      const HloSharding& input_spec =
          src_strategy_group->strategies[sid].output_sharding;
      // There is no way for us to handle manual sharding.
      if (input_spec.IsManual() || input_spec.IsManualSubgroup()) {
        continue;
      }

      HloSharding output_spec = input_spec;
      if (!(input_spec.IsReplicated() || input_spec.IsTileMaximal())) {
        // All 3. sub-cases (reduction dim would be replicated in the
        // output)
        output_spec = hlo_sharding_util::PartiallyReplicateTiledShardingOnDims(
            input_spec, {*reduction_dim});
      }

      // Get a list of input shardings, each corresponds to an operand.
      std::vector<std::optional<HloSharding>> input_shardings;
      for (int64_t k = 0; k < output_size * 2; ++k) {
        if (k < output_size) {
          input_shardings.push_back(input_spec);
        } else {
          input_shardings.push_back(HloSharding::Replicate());
        }
      }

      std::string name = ToStringSimple(output_spec);
      double compute_cost = 0, communication_cost = 0;
      double memory_cost = ByteSizeOfShapeWithSharding(
          ins->shape().tuple_shapes(i), output_spec);
      std::pair<ReshardingCosts, ReshardingCosts> resharding_costs =
          GenerateReshardingCostsAndMissingShardingsForAllOperands(
              ins, output_spec, strategy_map, cluster_env, call_graph,
              input_shardings);

      child_strategy_group->strategies.push_back(ShardingStrategy(
          {std::move(name), std::move(output_spec), compute_cost,
           communication_cost, memory_cost, std::move(resharding_costs.first),
           std::move(resharding_costs.second), std::move(input_shardings)}));
    }

    strategy_group->childs.push_back(std::move(child_strategy_group));
  }
  return strategy_group;
}

std::unique_ptr<StrategyGroup> MaybeFollowInsStrategyGroup(
    const StrategyGroup* src_strategy_group, const Shape& shape,
    const size_t instruction_id, StrategyGroups& strategy_groups,
    const ClusterEnvironment& cluster_env,
    const StableHashMap<NodeIdx, std::vector<ShardingStrategy>>&
        pretrimmed_strategy_map) {
  std::unique_ptr<StrategyGroup> strategy_group;
  if (src_strategy_group->is_tuple) {
    CHECK(shape.IsTuple());
    CHECK_EQ(shape.tuple_shapes_size(), src_strategy_group->childs.size());
    strategy_group = CreateTupleStrategyGroup(instruction_id);
    strategy_group->childs.reserve(src_strategy_group->childs.size());
    for (size_t i = 0; i < src_strategy_group->childs.size(); ++i) {
      auto child_strategies = MaybeFollowInsStrategyGroup(
          src_strategy_group->childs[i].get(), shape.tuple_shapes(i),
          instruction_id, strategy_groups, cluster_env,
          pretrimmed_strategy_map);
      child_strategies->tuple_element_idx = i;
      strategy_group->childs.push_back(std::move(child_strategies));
    }
  } else {
    strategy_group =
        CreateLeafStrategyGroupWithoutInNodes(instruction_id, strategy_groups);
    strategy_group->in_nodes.push_back(src_strategy_group);
    FollowArrayOrTokenStrategyGroup(*src_strategy_group, shape, instruction_id,
                                    cluster_env, pretrimmed_strategy_map,
                                    *strategy_group);
  }
  return strategy_group;
}

absl::StatusOr<std::unique_ptr<StrategyGroup>> FollowReduceStrategy(
    const HloInstruction* ins, const Shape& output_shape,
    const HloInstruction* operand, const HloInstruction* unit,
    const size_t instruction_id, StrategyMap& strategy_map,
    StrategyGroups& strategy_groups, const ClusterEnvironment& cluster_env,
    const bool allow_mixed_mesh_shape, const bool crash_at_error) {
  std::unique_ptr<StrategyGroup> strategy_group;
  if (output_shape.IsTuple()) {
    strategy_group = CreateTupleStrategyGroup(instruction_id);
    strategy_group->childs.reserve(ins->shape().tuple_shapes_size());
    for (size_t i = 0; i < ins->shape().tuple_shapes_size(); ++i) {
      auto child_strategy_status = FollowReduceStrategy(
          ins, ins->shape().tuple_shapes().at(i), ins->operand(i),
          ins->operand(i + ins->shape().tuple_shapes_size()), instruction_id,
          strategy_map, strategy_groups, cluster_env, allow_mixed_mesh_shape,
          crash_at_error);
      if (!child_strategy_status.ok()) {
        return child_strategy_status;
      }
      child_strategy_status.value()->tuple_element_idx = i;
      strategy_group->childs.push_back(
          std::move(child_strategy_status.value()));
    }
  } else if (output_shape.IsArray()) {
    strategy_group = CreateLeafStrategyGroup(instruction_id, ins, strategy_map,
                                             strategy_groups);
    const StrategyGroup* src_strategy_group = strategy_map.at(operand).get();
    // Follows the strategy of the operand.
    strategy_group->following = src_strategy_group;
    strategy_group->strategies.reserve(src_strategy_group->strategies.size());
    // Map operand dims to inst dim
    // Example: f32[1,16]{1,0} reduce(f32[1,16,4096]{2,1,0} %param0,
    //                               f32[] %param1), dimensions={2}
    // op_dim_to_output_dim = [0, 1, -1]
    std::vector<int64_t> op_dim_to_output_dim =
        GetDimensionMapping(/*reduced_dimensions=*/ins->dimensions(),
                            /*op_count*/ operand->shape().rank());
    CHECK_EQ(ins->dimensions().size() + output_shape.rank(),
             operand->shape().rank())
        << "Invalid kReduce: output size + reduced dimensions size != op count";

    for (size_t sid = 0; sid < src_strategy_group->strategies.size(); ++sid) {
      HloSharding input_sharding =
          src_strategy_group->strategies[sid].output_sharding;
      const auto& tensor_dim_to_mesh = cluster_env.GetTensorDimToMeshDimWrapper(
          operand->shape(), input_sharding,
          /* consider_reverse_device_meshes */ true,
          /* crash_at_error */ crash_at_error);
      if (tensor_dim_to_mesh.size() != operand->shape().rank()) {
        return absl::InvalidArgumentError(
            "Cannot generate tensor dim to mesh dim mapping");
      }
      std::vector<int64_t> all_reduce_dims;
      for (int64_t op_dim = 0; op_dim < operand->shape().rank(); ++op_dim) {
        int64_t mesh_dim = tensor_dim_to_mesh[op_dim];
        // Replicates on this mesh dim.
        if (mesh_dim == -1) {
          continue;
        }
        if (op_dim_to_output_dim[op_dim] == -1) {
          // Reduce on a split dim. Require an allreduce
          all_reduce_dims.push_back(mesh_dim);
        }
      }
      std::unique_ptr<HloInstruction> operand_clone = operand->Clone();
      std::unique_ptr<HloInstruction> unit_clone = unit->Clone();
      // Creates a new reduce op with one output, which is easier to use
      // GetShardingFromUser() to get the input sharding.
      std::unique_ptr<HloInstruction> new_reduce = HloInstruction::CreateReduce(
          output_shape, operand_clone.get(), unit_clone.get(),
          ins->dimensions(), ins->to_apply());
      operand_clone->set_sharding(
          src_strategy_group->strategies[sid].output_sharding);
      absl::Status s = new_reduce->ReplaceOperandWith(0, operand_clone.get());
      if (!s.ok()) {
        continue;
      }
      CHECK(InferReduceShardingFromOperand(new_reduce.get(), false, true));
      HloSharding output_spec = new_reduce->sharding();
      new_reduce.reset();
      operand_clone.reset();
      unit_clone.reset();

      const std::string name = ToStringSimple(output_spec);

      double compute_cost = 0, communication_cost = 0;
      double memory_cost =
          ByteSizeOfShapeWithSharding(output_shape, output_spec);
      for (int64_t mesh_dim : all_reduce_dims) {
        communication_cost += cluster_env.AllReduceCost(memory_cost, mesh_dim);
      }
      ReshardingCosts communication_resharding_costs;
      ReshardingCosts memory_resharding_costs;
      for (int64_t k = 0; k < ins->operand_count(); ++k) {
        const HloInstruction* cur_operand = ins->operand(k);
        if (ToString(cur_operand->shape().dimensions()) ==
            ToString(operand->shape().dimensions())) {
          const StrategyGroup* operand_strategies =
              strategy_map.at(cur_operand).get();
          communication_resharding_costs.push_back(
              CommunicationReshardingCostVector(operand_strategies,
                                                cur_operand->shape(),
                                                input_sharding, cluster_env));
          memory_resharding_costs.push_back(MemoryReshardingCostVector(
              operand_strategies, cur_operand->shape(), input_sharding,
              cluster_env));
        } else {
          communication_resharding_costs.push_back(std::vector<double>(
              strategy_map.at(cur_operand)->strategies.size(), 0.0));
          memory_resharding_costs.push_back(std::vector<double>(
              strategy_map.at(cur_operand)->strategies.size(), 0.0));
        }
      }
      const ShardingStrategy strategy =
          ShardingStrategy({name,
                            output_spec,
                            compute_cost,
                            communication_cost,
                            memory_cost,
                            communication_resharding_costs,
                            memory_resharding_costs,
                            {input_sharding}});
      strategy_group->strategies.push_back(strategy);
    }
  } else {
    LOG(FATAL) << "Unhandled kReduce shape: " << ins->shape().ToString();
  }
  return strategy_group;
}

std::vector<size_t> FindReplicateStrategyIndices(
    const std::vector<ShardingStrategy>& strategies) {
  std::vector<size_t> indices;
  for (size_t i = 0; i < strategies.size(); i++) {
    if (strategies.at(i).output_sharding.IsReplicated()) {
      indices.push_back(i);
    }
  }
  return indices;
}

std::tuple<ReshardingCosts, ReshardingCosts,
           std::vector<std::optional<HloSharding>>>
ReshardingCostsForTupleOperand(const HloInstruction* operand,
                               StrategyGroup* operand_strategy_vector) {
  // TODO(yuemmawang) Support instructions with more than one tuple operand.
  // Creates resharding costs such that favors when operand strategies are
  // replicated.
  ReshardingCosts communication_resharding_costs;
  ReshardingCosts memory_resharding_costs;
  std::vector<HloSharding> tuple_element_shardings;
  for (size_t tuple_element_idx = 0;
       tuple_element_idx < operand->shape().tuple_shapes_size();
       tuple_element_idx++) {
    auto tuple_element_strategies =
        operand_strategy_vector->childs.at(tuple_element_idx).get();
    std::vector<size_t> indices =
        FindReplicateStrategyIndices(tuple_element_strategies->strategies);
    CHECK_GT(indices.size(), 0)
        << "There is no replicated strategy in instruction "
        << operand->ToString() << ".\nStrategies:\n"
        << tuple_element_strategies->ToString();
    memory_resharding_costs.push_back(
        std::vector<double>(tuple_element_strategies->strategies.size(), 0));
    communication_resharding_costs.push_back(std::vector<double>(
        tuple_element_strategies->strategies.size(), kInfinityCost));
    tuple_element_shardings.push_back(HloSharding::Replicate());
    for (const size_t i : indices) {
      communication_resharding_costs.back().at(i) = 0.0;
    }
  }
  return {communication_resharding_costs, memory_resharding_costs,
          std::vector<std::optional<HloSharding>>(
              {HloSharding::Tuple(operand->shape(), tuple_element_shardings)})};
}

ReshardingCosts CreateZeroReshardingCostsForAllOperands(
    const HloInstruction* ins, const StrategyMap& strategy_map) {
  ReshardingCosts resharding_costs;
  for (size_t i = 0; i < ins->operand_count(); ++i) {
    auto operand = ins->operand(i);
    const auto& operand_strategies = strategy_map.at(operand);
    if (operand->shape().IsTuple()) {
      if (ins->opcode() == HloOpcode::kConditional ||
          ins->opcode() == HloOpcode::kOutfeed) {
        resharding_costs.push_back(std::vector<double>(1, 0));
      } else {
        CHECK_EQ(ins->operand_count(), 0)
            << "Do not support instructions with more than one tuple "
               "operand.";
        for (size_t tuple_element_idx = 0;
             tuple_element_idx < operand->shape().tuple_shapes_size();
             tuple_element_idx++) {
          auto tuple_element_strategies =
              operand_strategies->childs.at(tuple_element_idx).get();
          resharding_costs.push_back(std::vector<double>(
              tuple_element_strategies->strategies.size(), 0));
        }
      }
    } else {
      resharding_costs.push_back(
          std::vector<double>(operand_strategies->strategies.size(), 0));
    }
  }
  return resharding_costs;
}

void GenerateOutfeedStrategy(const HloInstruction* ins, const Shape& shape,
                             const ClusterEnvironment& cluster_env,
                             const StrategyMap& strategy_map,
                             std::unique_ptr<StrategyGroup>& strategy_group,
                             const double replicated_penalty) {
  HloSharding output_spec = HloSharding::Replicate();
  ReshardingCosts communication_resharding_costs;
  ReshardingCosts memory_resharding_costs;
  std::vector<std::optional<HloSharding>> input_shardings;

  const int tuple_size = ins->operand(0)->shape().tuple_shapes_size();
  if (ins->has_sharding()) {
    std::vector<Shape> operand_shapes(ins->operand_count());
    for (int i = 0; i < ins->operand_count(); ++i) {
      operand_shapes[i] = ins->operand(i)->shape();
    }
    auto all_operands_tuple_shape = ShapeUtil::MakeTupleShape(operand_shapes);
    auto get_input_sharding = [&](int index) {
      auto sharding = ins->sharding();
      if (sharding.IsTuple()) {
        return (index >= 0)
                   ? sharding.GetSubSharding(all_operands_tuple_shape,
                                             {0, static_cast<int64_t>(index)})
                   : sharding.GetSubSharding(all_operands_tuple_shape, {1});
      } else {
        return sharding;
      }
    };

    for (size_t i = 0; i < tuple_size; ++i) {
      auto input_sharding = get_input_sharding(i);
      input_shardings.push_back(input_sharding);
      communication_resharding_costs.push_back(
          CommunicationReshardingCostVector(
              strategy_map.at(ins->operand(0))->childs[i].get(),
              ins->operand(0)->shape().tuple_shapes(i), input_sharding,
              cluster_env));
      memory_resharding_costs.push_back(MemoryReshardingCostVector(
          strategy_map.at(ins->operand(0))->childs[i].get(),
          ins->operand(0)->shape().tuple_shapes(i), input_sharding,
          cluster_env));
    }
    auto input_sharding = get_input_sharding(-1);
    input_shardings.push_back(input_sharding);
  } else {
    for (size_t i = 0; i < tuple_size; ++i) {
      communication_resharding_costs.push_back(std::vector<double>(
          strategy_map.at(ins->operand(0))->childs[i].get()->strategies.size(),
          0));
      memory_resharding_costs.push_back(std::vector<double>(
          strategy_map.at(ins->operand(0))->childs[i].get()->strategies.size(),
          0));
    }
  }
  communication_resharding_costs.push_back({});
  memory_resharding_costs.push_back({});
  double memory_cost = ByteSizeOfShapeWithSharding(shape, output_spec);
  strategy_group->strategies.push_back(
      ShardingStrategy({"R", HloSharding::Replicate(), replicated_penalty, 0,
                        memory_cost, std::move(communication_resharding_costs),
                        std::move(memory_resharding_costs), input_shardings}));
}

double ComputeCommunicationCost(
    const HloInstruction* ins,
    const std::vector<std::optional<HloSharding>>& operand_shardings,
    const ClusterEnvironment& cluster_env) {
  switch (ins->opcode()) {
    case HloOpcode::kGather: {
      if (operand_shardings[0].has_value() &&
          !operand_shardings[0]->IsReplicated()) {
        auto mesh_shape = cluster_env.device_mesh_.dimensions();
        auto mesh_dim = std::distance(
            mesh_shape.begin(),
            std::max_element(mesh_shape.begin(), mesh_shape.end()));
        // As seen in the test
        // SpmdPartitioningTest.GatherPartitionedOnTrivialSliceDims (in file
        // third_party/tensorflow/compiler/xla/service/spmd/spmd_partitioner_test.cc),
        // when the gather op is replicated and the first operand sharded, we
        // need an AllReduce to implement the gather op. We capture that cost
        // here.
        // TODO(pratikf) Model gather communication costs in a more principled
        // and exhaustive manner.
        return cluster_env.AllReduceCost(ByteSizeOfShape(ins->shape()),
                                         mesh_dim);
      }
      return 0;
    }
    default:
      LOG(FATAL) << "Unhandled instruction " << ins->ToString();
  }
}

// Add "Replicate()" strategy
// By default, when adding a replicated strategy for an op, we specify that all
// its operands need to be replicated as well (via the input_shardings field on
// a ShardingStrategy). When operands_to_consider_all_strategies_for is
// non-empty however, instead of merely allowing the operands to be replicated,
// we allos greater freedom for the shardings of the operands included in the
// set. More specifically, for these operands, we consider all generated
// strategies for those operands (instead of just replication) as potentially
// allowable shardings.
void AddReplicatedStrategy(
    const HloInstruction* ins, const Shape& shape,
    const ClusterEnvironment& cluster_env, const StrategyMap& strategy_map,
    std::unique_ptr<StrategyGroup>& strategy_group,
    const double replicated_penalty,
    absl::flat_hash_set<int64_t> operands_to_consider_all_strategies_for) {
  HloSharding replicated_strategy = HloSharding::Replicate();
  HloSharding output_spec = replicated_strategy;
  double memory_cost = ByteSizeOfShapeWithSharding(shape, output_spec);

  CHECK_LE(operands_to_consider_all_strategies_for.size(), 1);
  if (!operands_to_consider_all_strategies_for.empty()) {
    int64_t operand_to_consider_all_strategies_for =
        *operands_to_consider_all_strategies_for.begin();
    auto operand = ins->operand(operand_to_consider_all_strategies_for);
    CHECK(!operand->shape().IsTuple());
    auto operand_strategies_to_consider = strategy_map.at(operand).get();
    std::vector<std::vector<std::optional<HloSharding>>>
        possible_input_shardings(
            operand_strategies_to_consider->strategies.size(),
            std::vector<std::optional<HloSharding>>(ins->operand_count()));
    std::vector<ReshardingCosts> possible_communication_resharding_costs(
        operand_strategies_to_consider->strategies.size(),
        ReshardingCosts(ins->operand_count()));
    std::vector<ReshardingCosts> possible_memory_resharding_costs(
        operand_strategies_to_consider->strategies.size(),
        ReshardingCosts(ins->operand_count()));

    for (int64_t k = 0; k < ins->operand_count(); ++k) {
      CHECK(!ins->operand(k)->shape().IsTuple());
      if (k == operand_to_consider_all_strategies_for) {
        CHECK_EQ(possible_input_shardings.size(),
                 operand_strategies_to_consider->strategies.size());
        for (size_t j = 0; j < possible_input_shardings.size(); ++j) {
          possible_input_shardings[j][k] =
              operand_strategies_to_consider->strategies[j].output_sharding;
          possible_communication_resharding_costs[j][k] =
              CommunicationReshardingCostVector(
                  strategy_map.at(ins->operand(k)).get(),
                  ins->operand(k)->shape(),
                  operand_strategies_to_consider->strategies[j].output_sharding,
                  cluster_env);
          possible_memory_resharding_costs[j][k] = MemoryReshardingCostVector(
              strategy_map.at(ins->operand(k)).get(), ins->operand(k)->shape(),
              operand_strategies_to_consider->strategies[j].output_sharding,
              cluster_env);
        }
      } else {
        for (size_t j = 0; j < possible_input_shardings.size(); ++j) {
          possible_input_shardings[j][k] = replicated_strategy;
          possible_communication_resharding_costs[j][k] =
              CommunicationReshardingCostVector(
                  strategy_map.at(ins->operand(k)).get(),
                  ins->operand(k)->shape(), replicated_strategy, cluster_env);
          possible_memory_resharding_costs[j][k] = MemoryReshardingCostVector(
              strategy_map.at(ins->operand(k)).get(), ins->operand(k)->shape(),
              replicated_strategy, cluster_env);
        }
      }
    }

    for (size_t j = 0; j < possible_input_shardings.size(); ++j) {
      double communication_cost = ComputeCommunicationCost(
          ins, possible_input_shardings[j], cluster_env);
      strategy_group->strategies.push_back(ShardingStrategy(
          {"R", replicated_strategy, replicated_penalty, communication_cost,
           memory_cost, std::move(possible_communication_resharding_costs[j]),
           std::move(possible_memory_resharding_costs[j]),
           std::move(possible_input_shardings[j])}));
    }
  } else {
    ReshardingCosts communication_resharding_costs;
    ReshardingCosts memory_resharding_costs;
    std::vector<std::optional<HloSharding>> input_shardings;

    if (ins->operand_count() > 0 && ins->operand(0)->shape().IsTuple()) {
      CHECK_EQ(ins->operand_count(), 1)
          << "Do not support instructions with more than one tuple "
             "operand. If this CHECK fails, we will need to fix "
             "b/233412625.";
      std::tie(communication_resharding_costs, memory_resharding_costs,
               input_shardings) =
          ReshardingCostsForTupleOperand(
              ins->operand(0), strategy_map.at(ins->operand(0)).get());
    } else {
      for (int64_t k = 0; k < ins->operand_count(); ++k) {
        auto operand = ins->operand(k);
        if (ins->opcode() == HloOpcode::kConditional) {
          communication_resharding_costs.push_back(std::vector<double>(
              strategy_map.at(operand)->strategies.size(), 0));
          memory_resharding_costs.push_back(std::vector<double>(
              strategy_map.at(operand)->strategies.size(), 0));
        } else {
          communication_resharding_costs.push_back(
              CommunicationReshardingCostVector(strategy_map.at(operand).get(),
                                                ins->operand(k)->shape(),
                                                output_spec, cluster_env));
          memory_resharding_costs.push_back(MemoryReshardingCostVector(
              strategy_map.at(operand).get(), ins->operand(k)->shape(),
              output_spec, cluster_env));
          input_shardings.push_back(output_spec);
        }
      }
    }
    strategy_group->strategies.push_back(ShardingStrategy(
        {"R", HloSharding::Replicate(), replicated_penalty, 0, memory_cost,
         std::move(communication_resharding_costs),
         std::move(memory_resharding_costs), input_shardings}));
  }
}

// TODO(pratikf) Communication costs for sort HLO ops. This is currently a
// placeholder approximation and should be improved.
double ComputeSortCommunicationCost(const int64_t sort_dim,
                                    const int64_t operand_sharded_dim,
                                    const int64_t mesh_sharding_dim,
                                    const Shape& shape,
                                    const ClusterEnvironment& cluster_env) {
  if (sort_dim == operand_sharded_dim) {
    return cluster_env.AllToAllCost(ByteSizeOfShape(shape), mesh_sharding_dim);
  }
  return 0;
}

// Enumerate all 1d partition strategies.
void EnumerateAll1DPartition(const HloInstruction* ins, const Shape& shape,
                             const Array<int64_t>& device_mesh,
                             const ClusterEnvironment& cluster_env,
                             const StrategyMap& strategy_map,
                             std::unique_ptr<StrategyGroup>& strategy_group,
                             const bool only_allow_divisible,
                             const std::string& suffix,
                             const CallGraph& call_graph) {
  for (int64_t i = 0; i < shape.rank(); ++i) {
    for (int64_t j = 0; j < device_mesh.num_dimensions(); ++j) {
      if (device_mesh.dim(j) == 1 || shape.dimensions(i) < device_mesh.dim(j) ||
          (only_allow_divisible &&
           !IsDivisible(shape.dimensions(i), device_mesh.dim(j)))) {
        continue;
      }

      const std::string name = absl::StrFormat("S%d @ %d", i, j) + suffix;
      HloSharding output_spec = Tile(shape, {i}, {j}, device_mesh);
      double compute_cost = 0, communication_cost = 0;
      double memory_cost = ByteSizeOfShapeWithSharding(shape, output_spec);

      ReshardingCosts communication_resharding_costs;
      ReshardingCosts memory_resharding_costs;
      std::vector<std::optional<HloSharding>> input_shardings;
      if (ins->opcode() == HloOpcode::kConditional) {
        // TODO(pratikf): Compute input_shardings for kConditional ops
        communication_resharding_costs =
            CreateZeroReshardingCostsForAllOperands(ins, strategy_map);
        memory_resharding_costs =
            CreateZeroReshardingCostsForAllOperands(ins, strategy_map);
      } else if (ins->operand_count() > 0 &&
                 ins->operand(0)->shape().IsTuple()) {
        CHECK_EQ(ins->operand_count(), 1)
            << "Do not support instructions with more than one tuple "
               "operand.";
        std::tie(communication_resharding_costs, memory_resharding_costs,
                 input_shardings) =
            ReshardingCostsForTupleOperand(
                ins->operand(0), strategy_map.at(ins->operand(0)).get());
      } else if (ins->opcode() == HloOpcode::kRngBitGenerator &&
                 ins->operand(0)->shape().IsArray()) {
        input_shardings.push_back(HloSharding::Replicate());
        std::tie(communication_resharding_costs, memory_resharding_costs) =
            GenerateReshardingCostsAndMissingShardingsForAllOperands(
                ins, output_spec, strategy_map, cluster_env, call_graph,
                input_shardings);
      } else {
        std::tie(communication_resharding_costs, memory_resharding_costs,
                 input_shardings) =
            GenerateReshardingCostsAndShardingsForAllOperands(
                ins, output_spec, strategy_map, cluster_env, call_graph);
      }
      if (ins->opcode() == HloOpcode::kSort) {
        auto sort_ins = xla::DynCast<HloSortInstruction>(ins);
        CHECK(sort_ins);
        communication_cost = ComputeSortCommunicationCost(
            sort_ins->sort_dimension(), i, j, shape, cluster_env);
      } else if (IsTopKCustomCall(ins)) {
        // TODO(pratikf) Better model topk communication costs. Currently using
        // the cost model for sort (which, as noted above in the comments for
        // the function) is also an approximation.
        communication_cost = ComputeSortCommunicationCost(
            ins->operand(0)->shape().rank() - 1, i, j, shape, cluster_env);
      }
      strategy_group->strategies.push_back(ShardingStrategy(
          {name, output_spec, compute_cost, communication_cost, memory_cost,
           std::move(communication_resharding_costs),
           std::move(memory_resharding_costs), input_shardings}));
    }
  }
}

void BuildStrategyAndCostForOp(const HloInstruction* ins, const Shape& shape,
                               const Array<int64_t>& device_mesh,
                               const ClusterEnvironment& cluster_env,
                               const StrategyMap& strategy_map,
                               std::unique_ptr<StrategyGroup>& strategy_group,
                               const CallGraph& call_graph,
                               absl::Span<const int64_t> tensor_dims);

void EnumerateAllPartition(const HloInstruction* ins, const Shape& shape,
                           const Array<int64_t>& device_mesh,
                           const ClusterEnvironment& cluster_env,
                           const StrategyMap& strategy_map,
                           std::unique_ptr<StrategyGroup>& strategy_group,
                           const InstructionBatchDimMap& batch_dim_map,
                           const bool only_allow_divisible,
                           const CallGraph& call_graph,
                           const int64_t partition_dimensions,
                           const std::vector<int64_t>& tensor_dims) {
  const auto tensor_dims_size = tensor_dims.size();
  if (tensor_dims_size == partition_dimensions) {
    BuildStrategyAndCostForOp(ins, shape, device_mesh, cluster_env,
                              strategy_map, strategy_group, call_graph,
                              tensor_dims);
    return;
  }
  auto iter = batch_dim_map.find(GetBatchDimMapKey(ins));
  int64_t batch_dim = -1;
  if (iter != batch_dim_map.end()) {
    batch_dim = iter->second;
  }
  // Fully tile the buffer to the mesh
  for (int64_t i = 0; i < shape.rank(); ++i) {
    auto tensor_it = std::find(tensor_dims.begin(), tensor_dims.end(), i);
    if ((batch_dim != -1 && batch_dim != i) || tensor_it != tensor_dims.end()) {
      continue;
    }
    if (shape.dimensions(i) < device_mesh.dim(tensor_dims_size)) {
      continue;
    }
    if (only_allow_divisible &&
        !IsDivisible(shape.dimensions(i), device_mesh.dim(tensor_dims_size))) {
      continue;
    }
    std::vector<int64_t> next_tensor_dims = tensor_dims;
    next_tensor_dims.push_back(i);
    EnumerateAllPartition(ins, shape, device_mesh, cluster_env, strategy_map,
                          strategy_group, batch_dim_map, only_allow_divisible,
                          call_graph, partition_dimensions, next_tensor_dims);
  }
}

void BuildStrategyAndCostForOp(const HloInstruction* ins, const Shape& shape,
                               const Array<int64_t>& device_mesh,
                               const ClusterEnvironment& cluster_env,
                               const StrategyMap& strategy_map,
                               std::unique_ptr<StrategyGroup>& strategy_group,
                               const CallGraph& call_graph,
                               absl::Span<const int64_t> tensor_dims) {
  std::vector<int64_t> mesh_dims(tensor_dims.size());
  std::iota(mesh_dims.begin(), mesh_dims.end(), 0);
  const std::string name =
      absl::StrFormat("S{%s} @ {%s}", absl::StrJoin(tensor_dims, ","),
                      absl::StrJoin(mesh_dims, ","));
  HloSharding output_spec = Tile(shape, tensor_dims, mesh_dims, device_mesh);
  double compute_cost = 0, communication_cost = 0;
  double memory_cost = ByteSizeOfShapeWithSharding(shape, output_spec);
  std::vector<std::optional<HloSharding>> input_shardings;
  ReshardingCosts communication_resharding_costs;
  ReshardingCosts memory_resharding_costs;
  if (ins->opcode() == HloOpcode::kConditional) {
    // TODO(pratikf): Compute input_shardings for kConditional ops
    communication_resharding_costs =
        CreateZeroReshardingCostsForAllOperands(ins, strategy_map);
    memory_resharding_costs =
        CreateZeroReshardingCostsForAllOperands(ins, strategy_map);
  } else if (ins->operand_count() > 0 && ins->operand(0)->shape().IsTuple()) {
    CHECK_EQ(ins->operand_count(), 1)
        << "Do not support instructions with more than one tuple "
           "operand. If this CHECK fails, we will need to fix "
           "b/233412625.";
    std::tie(communication_resharding_costs, memory_resharding_costs,
             input_shardings) =
        ReshardingCostsForTupleOperand(ins->operand(0),
                                       strategy_map.at(ins->operand(0)).get());
  } else {
    std::tie(communication_resharding_costs, memory_resharding_costs,
             input_shardings) =
        GenerateReshardingCostsAndShardingsForAllOperands(
            ins, output_spec, strategy_map, cluster_env, call_graph);
  }
  // TODO(pratikf) Communication costs for sort HLO ops. This is currently a
  // placeholder approximation and should be improved.
  int64_t sort_or_topk_dim = -1;
  if (ins->opcode() == HloOpcode::kSort) {
    auto sort_ins = xla::DynCast<HloSortInstruction>(ins);
    CHECK(sort_ins);
    sort_or_topk_dim = sort_ins->sort_dimension();
  } else if (IsTopKCustomCall(ins)) {
    sort_or_topk_dim = ins->operand(0)->shape().rank() - 1;
  }

  if (sort_or_topk_dim != -1) {
    if (auto index = GetIndex(tensor_dims, sort_or_topk_dim); index != -1) {
      communication_cost = ComputeSortCommunicationCost(
          sort_or_topk_dim, sort_or_topk_dim, index, shape, cluster_env);
    }
  }

  strategy_group->strategies.push_back(
      ShardingStrategy({name, output_spec, compute_cost, communication_cost,
                        memory_cost, std::move(communication_resharding_costs),
                        std::move(memory_resharding_costs), input_shardings}));
}

void EnumerateAll1DPartitionReshape(
    const HloInstruction* ins, const Array<int64_t>& device_mesh,
    const ClusterEnvironment& cluster_env, const StrategyMap& strategy_map,
    std::unique_ptr<StrategyGroup>& strategy_group, bool only_allow_divisible,
    const std::string& suffix) {
  const HloInstruction* operand = ins->operand(0);

  for (int64_t i = 0; i < ins->shape().rank(); ++i) {
    for (int64_t j = 0; j < device_mesh.num_dimensions(); ++j) {
      if (device_mesh.dim(j) == 1 ||
          (only_allow_divisible &&
           !IsDivisible(ins->shape().dimensions(i), device_mesh.dim(j)))) {
        continue;
      }
      HloSharding output_spec = Tile(ins->shape(), {i}, {j}, device_mesh);

      std::optional<HloSharding> input_spec =
          hlo_sharding_util::ReshapeSharding(ins->shape(), operand->shape(),
                                             output_spec);
      if (!input_spec.has_value()) {  // invalid reshape
        continue;
      }

      if (cluster_env.IsDeviceMesh1D() &&
          VectorGreaterThanOneElementCount(
              input_spec->tile_assignment().dimensions()) > 1) {
        continue;
      }

      const std::string name = absl::StrFormat("S%d @ %d", i, j) + suffix;
      double compute_cost = 0, communication_cost = 0;
      double memory_cost =
          ByteSizeOfShapeWithSharding(ins->shape(), output_spec);

      ReshardingCosts communication_resharding_costs{
          CommunicationReshardingCostVector(strategy_map.at(operand).get(),
                                            operand->shape(), *input_spec,
                                            cluster_env)};
      ReshardingCosts memory_resharding_costs{MemoryReshardingCostVector(
          strategy_map.at(operand).get(), operand->shape(), *input_spec,
          cluster_env)};
      strategy_group->strategies.push_back(
          ShardingStrategy({name,
                            output_spec,
                            compute_cost,
                            communication_cost,
                            memory_cost,
                            std::move(communication_resharding_costs),
                            std::move(memory_resharding_costs),
                            {*input_spec}}));
    }
  }
}

void BuildStrategyAndCostForReshape(
    const HloInstruction* ins, const Array<int64_t>& device_mesh,
    const ClusterEnvironment& cluster_env, const StrategyMap& strategy_map,
    std::unique_ptr<StrategyGroup>& strategy_group,
    absl::Span<const int64_t> tensor_dims);

// Enumerate all partitions for reshape. Batch dim is always partitioned.
void EnumeratePartitionReshape(const HloInstruction* ins,
                               const Array<int64_t>& device_mesh,
                               const ClusterEnvironment& cluster_env,
                               const StrategyMap& strategy_map,
                               const InstructionBatchDimMap& batch_dim_map,
                               std::unique_ptr<StrategyGroup>& strategy_group,
                               const bool only_allow_divisible,
                               const int64_t partition_dimensions,
                               const std::vector<int64_t>& tensor_dims = {}) {
  const auto tensor_dims_size = tensor_dims.size();
  if (tensor_dims_size == partition_dimensions) {
    BuildStrategyAndCostForReshape(ins, device_mesh, cluster_env, strategy_map,
                                   strategy_group, tensor_dims);
    return;
  }
  auto iter = batch_dim_map.find(GetBatchDimMapKey(ins));
  int64_t batch_dim = -1;
  if (iter != batch_dim_map.end()) {
    batch_dim = iter->second;
  }

  // Split batch dim + another dim
  for (int64_t i = 0; i < ins->shape().rank(); ++i) {
    auto tensor_it = std::find(tensor_dims.begin(), tensor_dims.end(), i);
    if ((batch_dim != -1 && batch_dim != i) || tensor_it != tensor_dims.end()) {
      continue;
    }
    if (ins->shape().dimensions(i) < device_mesh.dim(tensor_dims_size)) {
      continue;
    }
    if (only_allow_divisible &&
        !IsDivisible(ins->shape().dimensions(i),
                     device_mesh.dim(tensor_dims_size))) {
      continue;
    }

    std::vector<int64_t> next_tensor_dims = tensor_dims;
    next_tensor_dims.push_back(i);
    EnumeratePartitionReshape(ins, device_mesh, cluster_env, strategy_map,
                              batch_dim_map, strategy_group,
                              only_allow_divisible, partition_dimensions,
                              next_tensor_dims);
  }
}

void BuildStrategyAndCostForReshape(
    const HloInstruction* ins, const Array<int64_t>& device_mesh,
    const ClusterEnvironment& cluster_env, const StrategyMap& strategy_map,
    std::unique_ptr<StrategyGroup>& strategy_group,
    absl::Span<const int64_t> tensor_dims) {
  const HloInstruction* operand = ins->operand(0);
  std::vector<int64_t> mesh_dims(tensor_dims.size());
  std::iota(mesh_dims.begin(), mesh_dims.end(), 0);
  HloSharding output_spec =
      Tile(ins->shape(), tensor_dims, mesh_dims, device_mesh);
  std::optional<HloSharding> input_spec = hlo_sharding_util::ReshapeSharding(
      ins->shape(), operand->shape(), output_spec);
  if (!input_spec.has_value()) {  // invalid reshape
    return;
  }
  std::string name =
      absl::StrFormat("S%s @ {%s}", absl::StrJoin(tensor_dims, ""),
                      absl::StrJoin(mesh_dims, ","));
  double compute_cost = 0, communication_cost = 0;
  double memory_cost = ByteSizeOfShapeWithSharding(ins->shape(), output_spec);
  ;

  ReshardingCosts communication_resharding_costs{
      CommunicationReshardingCostVector(strategy_map.at(operand).get(),
                                        operand->shape(), *input_spec,
                                        cluster_env)};
  ReshardingCosts memory_resharding_costs{
      MemoryReshardingCostVector(strategy_map.at(operand).get(),
                                 operand->shape(), *input_spec, cluster_env)};
  strategy_group->strategies.push_back(
      ShardingStrategy({name,
                        output_spec,
                        compute_cost,
                        communication_cost,
                        memory_cost,
                        std::move(communication_resharding_costs),
                        std::move(memory_resharding_costs),
                        {*input_spec}}));
}

// Return the maximum number of tiles among all strategies of an instruction.
int64_t MaxNumTiles(const StrategyMap& strategy_map,
                    const HloInstruction* ins) {
  const StrategyGroup* strategy_group = strategy_map.at(ins).get();
  // TODO(zhuohan): optimize with path compression.
  while (strategy_group->following != nullptr) {
    strategy_group = strategy_group->following;
  }
  int64_t max_num_tiles = -1;
  for (size_t i = 0; i < strategy_group->strategies.size(); ++i) {
    max_num_tiles =
        std::max(max_num_tiles,
                 strategy_group->strategies[i].output_sharding.NumTiles());
  }
  return max_num_tiles;
}

// Choose an operand to follow. We choose to follow the operand with the highest
// priority. The priority is defined as a function of two entities as below:
//
// priority(operand) =
//   max(x.output_spec.num_tiles for x in operand.strategies) +
//   depth(operand) * depth_normalizer
//
// For example, We therefore prefer one operand with strategies with a high
// number of tiles and operands that have a higher depth in the graph. When the
// priorities are similar (within range_delta), we set these operators to be
// "tied" and let the ILP solver to pick which one to follow.
//
// The function returns (follow_idx, tie), where the follow_idx is the id of
// the operand to follow and tie is a boolean variable that indicates whether
// there are multiple operands have similar priority. Return `tie == True` if
// there are two operands with very close priorities and we cannot decide which
// one to follow.
std::pair<int64_t, bool> ChooseOperandToFollow(
    const StrategyMap& strategy_map, const InstructionDepthMap& depth_map,
    const AliasMap& alias_map, const int64_t max_depth,
    const HloInstruction* ins) {
  // If an alias constraint is set, always follow its alias source.
  auto it = alias_map.find(ins);
  if (it != alias_map.end()) {
    for (int64_t i = 0; i < ins->operand_count(); ++i) {
      const HloInstruction* operand = ins->operand(i);
      if (operand == it->second) {
        return {i, false};
      }
    }
  }

  std::optional<int64_t> follow_idx;
  bool tie = false;
  double max_priority = -1e20;
  double depth_normalizer = 0.1 / max_depth;
  double range_delta = 4 * depth_normalizer;

  for (int64_t i = 0; i < ins->operand_count(); ++i) {
    const HloInstruction* operand = ins->operand(i);
    double priority = MaxNumTiles(strategy_map, operand) +
                      depth_map.at(operand) * depth_normalizer;
    if (priority > max_priority + range_delta) {
      follow_idx = i;
      tie = false;
      max_priority = priority;
    } else if (priority >= max_priority - range_delta) {
      tie = true;
    }
  }
  CHECK(follow_idx.has_value());
  return {*follow_idx, tie};
}

// Return whether an instruction can follow one of its operand when more than
// one operand have the same priority.
// Consider adding special cases here if the auto sharding following strategy
// behaves weird for your model.
bool AllowTieFollowing(const HloInstruction* ins) {
  if (ins->opcode() == HloOpcode::kCompare ||
      ins->opcode() == HloOpcode::kAnd) {
    // This is used to resolve tricky cases in our testing models where an iota
    // and a parameter has the same priority when comparing to each other. This
    // happens for embedding, onehot or make_attention_mask.
    return false;
  }
  if (ins->operand_count() == 3) {
    return false;
  }
  return true;
}

// 1. Disable mixed mesh shape if the batch dim is not divisible by the
// number of devices.
// 2. Disable force_batch_dim_to_mesh_dim if the batch dim is 1. In this case,
// the batch dim analysis can be wrong because the batch dim might be dropped.
void DisableIncompatibleMixedMeshShapeAndForceBatchDim(
    const InstructionBatchDimMap& batch_dim_map,
    const std::vector<HloInstruction*>& instructions, int num_devices,
    AutoShardingOption& option) {
  int64_t batch_size = INT_MAX;
  for (const auto& iter : batch_dim_map) {
    batch_size = std::min(batch_size, FindInstruction(instructions, iter.first)
                                          ->shape()
                                          .dimensions(iter.second));
  }

  if (IsDivisible(batch_size, num_devices)) {
    if (option.allow_mixed_mesh_shape) {
      option.allow_mixed_mesh_shape = false;
      LOG(WARNING)
          << "Mixed mesh shape is disabled due to indivisible batch size.";
    }
  }

  if (batch_size == 1) {
    option.force_batch_dim_to_mesh_dim = -1;
  }
}

void FillAllStrategiesForArray(
    std::unique_ptr<StrategyGroup>& strategy_group, const HloInstruction* ins,
    const Shape& shape, const ClusterEnvironment& cluster_env,
    const StrategyMap& strategy_map, const AutoShardingOption& option,
    const double replicated_penalty,
    const InstructionBatchDimMap& batch_dim_map, const CallGraph& call_graph,
    const bool only_allow_divisible, const bool create_replicated_strategies,
    const bool create_partially_replicated_strategies) {
  if (create_partially_replicated_strategies || cluster_env.IsDeviceMesh1D()) {
    EnumerateAll1DPartition(ins, shape, cluster_env.device_mesh_, cluster_env,
                            strategy_map, strategy_group, only_allow_divisible,
                            "", call_graph);
  }
  // Split 2 dims
  if (cluster_env.IsDeviceMesh2D()) {
    EnumerateAllPartition(ins, shape, cluster_env.device_mesh_, cluster_env,
                          strategy_map, strategy_group, batch_dim_map,
                          only_allow_divisible, call_graph, /*partitions*/ 2);
  }
  // Split 3 dims
  if (cluster_env.IsDeviceMesh3D()) {
    EnumerateAllPartition(ins, shape, cluster_env.device_mesh_, cluster_env,
                          strategy_map, strategy_group, batch_dim_map,
                          only_allow_divisible, call_graph, /*partitions*/ 3);
  }

  if (option.allow_mixed_mesh_shape && cluster_env.IsDeviceMesh2D()) {
    // Set penalty for 1d partial tiled layout
    for (size_t i = 0; i < strategy_group->strategies.size(); ++i) {
      strategy_group->strategies[i].compute_cost += replicated_penalty * 0.8;
    }

    // Split 1 dim, but for 1d mesh
    EnumerateAll1DPartition(ins, shape, cluster_env.device_mesh_1d_,
                            cluster_env, strategy_map, strategy_group,
                            only_allow_divisible, " 1d", call_graph);
  }
  if (create_replicated_strategies || strategy_group->strategies.empty()) {
    AddReplicatedStrategy(ins, shape, cluster_env, strategy_map, strategy_group,
                          replicated_penalty);
  }

  // If force_batch_dim_to_mesh_dim is set, filter out invalid strategies
  // and only keep the data parallel strategies.
  if (option.force_batch_dim_to_mesh_dim >= 0 &&
      batch_dim_map.contains(GetBatchDimMapKey(ins))) {
    CHECK_OK(FilterStrategy(ins, shape, strategy_group, cluster_env,
                            batch_dim_map, option));
  }
}

absl::StatusOr<std::unique_ptr<StrategyGroup>> CreateAllStrategiesGroup(
    const HloInstruction* ins, const Shape& shape, const size_t instruction_id,
    StrategyGroups& strategy_groups, const ClusterEnvironment& cluster_env,
    const StrategyMap& strategy_map, const AutoShardingOption& option,
    const double replicated_penalty,
    const InstructionBatchDimMap& batch_dim_map, const CallGraph& call_graph,
    const bool only_allow_divisible, const bool create_replicated_strategies,
    const bool create_partially_replicated_strategies) {
  std::unique_ptr<StrategyGroup> strategy_group;
  if (shape.IsTuple()) {
    strategy_group = CreateTupleStrategyGroup(instruction_id);
    strategy_group->childs.reserve(shape.tuple_shapes_size());
    for (size_t i = 0; i < shape.tuple_shapes_size(); ++i) {
      auto child_strategies =
          CreateAllStrategiesGroup(ins, shape.tuple_shapes(i), instruction_id,
                                   strategy_groups, cluster_env, strategy_map,
                                   option, replicated_penalty, batch_dim_map,
                                   call_graph, only_allow_divisible,
                                   create_replicated_strategies,
                                   create_partially_replicated_strategies)
              .value();
      child_strategies->tuple_element_idx = i;
      strategy_group->childs.push_back(std::move(child_strategies));
    }
  } else if (shape.IsArray()) {
    strategy_group = CreateLeafStrategyGroup(instruction_id, ins, strategy_map,
                                             strategy_groups);

    FillAllStrategiesForArray(
        strategy_group, ins, shape, cluster_env, strategy_map, option,
        replicated_penalty, batch_dim_map, call_graph, only_allow_divisible,
        create_replicated_strategies, create_partially_replicated_strategies);
  } else if (shape.IsToken()) {
    strategy_group = CreateLeafStrategyGroup(instruction_id, ins, strategy_map,
                                             strategy_groups);
    AddReplicatedStrategy(ins, shape, cluster_env, strategy_map, strategy_group,
                          replicated_penalty);
  } else {
    LOG(FATAL) << "Unsupported instruction shape: " << shape.DebugString();
  }
  return strategy_group;
}

// The sharding is replicated or the total number of tiles is over or equal to
// the total number of devices. If returns true, this sharding is likely
// provided by users.
bool ShardingIsComplete(const HloSharding& sharding, size_t total_num_devices) {
  return sharding.TotalNumTiles() >= total_num_devices ||
         sharding.IsReplicated();
}

// Two shardings shard the same dimension of a given tensor.
bool ShardingIsConsistent(const HloSharding& partial_sharding,
                          const HloSharding& complete_sharding, bool strict) {
  if (partial_sharding.tile_assignment().num_dimensions() >
      complete_sharding.tile_assignment().num_dimensions()) {
    return false;
  }
  for (size_t i = 0; i < partial_sharding.tile_assignment().num_dimensions();
       ++i) {
    if (strict && partial_sharding.tile_assignment().dim(i) > 1 &&
        partial_sharding.tile_assignment().dim(i) ==
            complete_sharding.tile_assignment().dim(i)) {
      return true;
    }
    if (!strict && partial_sharding.tile_assignment().dim(i) > 1 &&
        complete_sharding.tile_assignment().dim(i) > 1) {
      return true;
    }
  }
  return false;
}

// Existing shardings refer to the HloSharding field in the given
// HloInstruction. This function handles two cases:
// 1. Existing sharding is from outside of XLA, which we refer to as user
// sharding. We need to preserve user shardings when the HloModule exit from
// AutoSharding.
// 2. Existing sharding is from previous iteration when
// solve_nd_sharding_iteratively is true. We use such shardings as hints to
// reduce the current iteration's problem size, by keeping sharding strategies
// that shard the same tensor dimensions as specified in the existing
// HloSharding.
// These two are distinguished by ShardingIsComplete().
void TrimOrGenerateStrategiesBasedOnExistingSharding(
    const Shape& output_shape, StrategyGroup* strategy_group,
    const StrategyMap& strategy_map,
    const std::vector<HloInstruction*>& instructions,
    const HloSharding& existing_sharding, const ClusterEnvironment& cluster_env,
    StableHashMap<int64_t, std::vector<ShardingStrategy>>&
        pretrimmed_strategy_map,
    const CallGraph& call_graph, const bool strict) {
  if (strategy_group->is_tuple) {
    for (size_t i = 0; i < strategy_group->childs.size(); ++i) {
      TrimOrGenerateStrategiesBasedOnExistingSharding(
          output_shape.tuple_shapes(i), strategy_group->childs.at(i).get(),
          strategy_map, instructions, existing_sharding.tuple_elements().at(i),
          cluster_env, pretrimmed_strategy_map, call_graph, strict);
    }
  } else {
    if (existing_sharding.IsUnknown()) {
      return;
    }
    if (ShardingIsComplete(existing_sharding,
                           cluster_env.device_mesh_.num_elements())) {
      // Sharding provided by XLA users, we need to keep them.
      strategy_group->following = nullptr;
      std::vector<ShardingStrategy> new_strategies;
      for (size_t i = 0; i < strategy_group->strategies.size(); i++) {
        if (strategy_group->strategies[i].output_sharding ==
            existing_sharding) {
          VLOG(1) << "Keeping strategy index: " << i;
          ShardingStrategy found_strategy = strategy_group->strategies[i];
          new_strategies.push_back(found_strategy);
        }
      }
      if (!new_strategies.empty()) {
        // Stores other strategies in the map, removes them in the vector and
        // only keeps the one we found.
        pretrimmed_strategy_map[strategy_group->node_idx] =
            strategy_group->strategies;
        strategy_group->strategies.clear();
        strategy_group->strategies = new_strategies;
      } else {
        VLOG(1) << "Generate a new strategy based on user sharding.";
        std::string name = ToStringSimple(existing_sharding);
        ReshardingCosts communication_resharding_costs;
        ReshardingCosts memory_resharding_costs;
        std::vector<std::optional<HloSharding>> input_shardings;
        if (!strategy_group->in_nodes.empty()) {
          HloInstruction* ins = instructions.at(strategy_group->instruction_id);
          for (size_t i = 0; i < strategy_group->in_nodes.size(); i++) {
            HloInstruction* operand =
                instructions.at(strategy_group->in_nodes.at(i)->instruction_id);
            std::optional<HloSharding> input_sharding =
                ShardingPropagation::GetShardingFromUser(
                    *operand, *ins, 10, true, call_graph,
                    /*sharding_helper=*/nullptr);
            StrategyGroup* operand_strategy_group =
                strategy_map.at(operand).get();
            Shape operand_shape = operand->shape();
            if (ins->opcode() == HloOpcode::kGetTupleElement) {
              if (input_sharding && input_sharding->IsTuple()) {
                input_sharding = input_sharding->GetSubSharding(
                    operand->shape(), {ins->tuple_index()});
              }
              operand_strategy_group =
                  operand_strategy_group->childs[ins->tuple_index()].get();
              operand_shape = operand->shape().tuple_shapes(ins->tuple_index());
            }

            if (!input_sharding) {
              if (existing_sharding.Validate(operand_shape).ok()) {
                input_sharding = existing_sharding;
              } else {
                input_sharding = HloSharding::Replicate();
              }
            }

            CHECK(input_sharding.has_value());

            input_shardings.push_back(*input_sharding);
            communication_resharding_costs.push_back(
                CommunicationReshardingCostVector(
                    operand_strategy_group, operand_shape, *input_sharding,
                    cluster_env));
            memory_resharding_costs.push_back(MemoryReshardingCostVector(
                operand_strategy_group, operand_shape, *input_sharding,
                cluster_env));
          }
        }
        double memory_cost =
            ByteSizeOfShapeWithSharding(output_shape, existing_sharding);
        if (!strategy_group->strategies.empty()) {
          pretrimmed_strategy_map[strategy_group->node_idx] =
              strategy_group->strategies;
        }
        strategy_group->strategies.clear();
        strategy_group->strategies.push_back(
            ShardingStrategy({name, existing_sharding, 0, 0, memory_cost,
                              communication_resharding_costs,
                              memory_resharding_costs, input_shardings}));
      }
      // If there is only one option for resharding, and the cost computed for
      // that option is kInfinityCost, set the cost to zero. This is okay
      // because there is only one option anyway, and having the costs set to
      // kInfinityCost is problematic for the solver.
      if (strategy_group->strategies.size() == 1) {
        for (auto& operand_communication_resharding_costs :
             strategy_group->strategies[0].communication_resharding_costs) {
          if (operand_communication_resharding_costs.size() == 1 &&
              operand_communication_resharding_costs[0] >= kInfinityCost) {
            operand_communication_resharding_costs[0] = 0;
          }
        }
      }
    } else if (!strategy_group->following) {
      // If existing sharding is a partial sharding from previous iteration,
      // find the strategies that are 1D&&complete or align with user
      // sharding.
      // It is IMPORTANT that we do this only for instructions that do no follow
      // others, to keep the number of ILP variable small.
      std::vector<ShardingStrategy> new_vector;
      for (const auto& strategy : strategy_group->strategies) {
        if (strategy.output_sharding.IsReplicated() ||
            ShardingIsConsistent(existing_sharding, strategy.output_sharding,
                                 strict) ||
            (VectorGreaterThanOneElementCount(
                 strategy.output_sharding.tile_assignment().dimensions()) ==
                 1 &&
             ShardingIsComplete(
                 strategy.output_sharding,
                 cluster_env.original_device_mesh_.num_elements()))) {
          new_vector.push_back(std::move(strategy));
        }
      }
      // If no sharding strategy left, just keep the original set, because we do
      // not have to strictly keep those shardings and the only purpose is to
      // reduce problem size for the last iteration.
      if (!new_vector.empty() &&
          new_vector.size() != strategy_group->strategies.size()) {
        strategy_group->following = nullptr;
        strategy_group->strategies = std::move(new_vector);
      }
    }
  }
}

void CheckMemoryCosts(StrategyGroup* strategy_group, const Shape& shape) {
  if (strategy_group->is_tuple) {
    for (size_t i = 0; i < strategy_group->childs.size(); i++) {
      CheckMemoryCosts(strategy_group->childs[i].get(),
                       shape.tuple_shapes().at(i));
    }
  } else {
    double full_mem = 0.0;
    for (const auto& strategy : strategy_group->strategies) {
      if (strategy.output_sharding.IsReplicated()) {
        full_mem = strategy.memory_cost;
        size_t size = ByteSizeOfShape(shape);
        CHECK_EQ(strategy.memory_cost, size);
      }
    }
    for (const auto& strategy : strategy_group->strategies) {
      if (!strategy.output_sharding.IsReplicated() && full_mem > 0.0) {
        CHECK_GE(strategy.memory_cost * strategy.output_sharding.NumTiles(),
                 full_mem);
      }
    }
  }
}

void RemoveShardingsWhereSmallDimsShardedAcrossManyDevices(
    const Shape& shape, StrategyGroup* strategy_group,
    const bool instruction_has_user_sharding) {
  if (strategy_group->is_tuple) {
    for (size_t i = 0; i < strategy_group->childs.size(); i++) {
      RemoveShardingsWhereSmallDimsShardedAcrossManyDevices(
          shape.tuple_shapes().at(i), strategy_group->childs[i].get(),
          instruction_has_user_sharding);
    }
  } else {
    if (instruction_has_user_sharding &&
        strategy_group->strategies.size() == 1) {
      // If an instruction has a specified user sharding, and there is only a
      // single strategy, removing that strategy would mean we won't have any
      // strategy for that instruction. Further, given that the user has
      // specified this sharding strategy, we should respect it, and hence not
      // remove it anyway.
      return;
    }
    std::vector<ShardingStrategy> new_vector;
    for (const auto& strategy : strategy_group->strategies) {
      if (strategy.output_sharding.IsReplicated()) {
        new_vector.push_back(strategy);
        continue;
      }

      const auto& tile_assignment = strategy.output_sharding.tile_assignment();
      bool is_strategy_valid = true;
      for (int64_t i = 0; i < shape.rank(); ++i) {
        if (tile_assignment.dim(i) > 1 &&
            tile_assignment.dim(i) > shape.dimensions(i)) {
          VLOG(1) << "May remove invalid strategy if valid ones exist: "
                  << strategy.ToString();
          is_strategy_valid = false;
          break;
        }
      }
      if (is_strategy_valid) {
        new_vector.push_back(strategy);
      }
    }
    if (!new_vector.empty()) {
      strategy_group->strategies = std::move(new_vector);
    }
  }
}

void CheckReshardingCostsShape(StrategyGroup* strategy_group) {
  if (strategy_group->is_tuple) {
    for (size_t i = 0; i < strategy_group->childs.size(); i++) {
      CheckReshardingCostsShape(strategy_group->childs[i].get());
    }
  } else {
    for (const auto& strategy : strategy_group->strategies) {
      if (strategy_group->in_nodes.size() == 1 &&
          strategy_group->in_nodes.at(0)->is_tuple) {
        // This is when current instruction's only operand is tuple, and the
        // first dimension of communication_resharding_costs should equal its
        // number of tuple elements.
        CHECK_EQ(strategy.communication_resharding_costs.size(),
                 strategy_group->in_nodes.at(0)->childs.size())
            << "Instruction ID: " << strategy_group->instruction_id << "\n"
            << strategy_group->ToString();
      } else {
        // The rest of the time, the first dimension of
        // communication_resharding_costs should equal its number of operands
        // (in_nodes).
        CHECK_EQ(strategy.communication_resharding_costs.size(),
                 strategy_group->in_nodes.size())
            << "Instruction ID: " << strategy_group->instruction_id << "\n"
            << strategy_group->ToString();
      }
      for (size_t i = 0; i < strategy.communication_resharding_costs.size();
           i++) {
        size_t to_compare;
        if (strategy_group->in_nodes.size() == 1 &&
            strategy_group->in_nodes.at(0)->is_tuple) {
          to_compare =
              strategy_group->in_nodes.at(0)->childs.at(i)->strategies.size();
        } else if (strategy_group->is_tuple) {
          to_compare = strategy_group->in_nodes.at(i)->childs.size();
        } else {
          to_compare = strategy_group->in_nodes.at(i)->strategies.size();
        }
        CHECK_EQ(strategy.communication_resharding_costs[i].size(), to_compare)
            << "\nIndex of communication_resharding_costs: " << i
            << "\nInstruction ID: " << strategy_group->instruction_id
            << "\nCurrent strategies:\n"
            << strategy_group->ToString();
      }
    }
  }
}

void ScaleCostsWithExecutionCounts(StrategyGroup* strategy_group,
                                   const int64_t execution_count) {
  if (strategy_group->is_tuple) {
    for (size_t i = 0; i < strategy_group->childs.size(); ++i) {
      ScaleCostsWithExecutionCounts(strategy_group->childs[i].get(),
                                    execution_count);
    }
  } else {
    for (auto& strategy : strategy_group->strategies) {
      strategy.compute_cost *= execution_count;
      strategy.communication_cost *= execution_count;
      for (auto i = 0; i < strategy.communication_resharding_costs.size();
           ++i) {
        for (auto j = 0; j < strategy.communication_resharding_costs[i].size();
             ++j) {
          strategy.communication_resharding_costs[i][j] *= execution_count;
        }
      }
    }
  }
}

std::unique_ptr<StrategyGroup> CreateElementwiseOperatorStrategies(
    const size_t instruction_id, const HloInstruction* ins,
    const StrategyMap& strategy_map, const ClusterEnvironment& cluster_env,
    const InstructionDepthMap& depth_map, const AliasMap& alias_map,
    const StableHashMap<int64_t, std::vector<ShardingStrategy>>&
        pretrimmed_strategy_map,
    const int64_t max_depth, StrategyGroups& strategy_groups,
    AssociativeDotPairs& associative_dot_pairs) {
  std::unique_ptr<StrategyGroup> strategy_group = CreateLeafStrategyGroup(
      instruction_id, ins, strategy_map, strategy_groups);

  // Choose an operand to follow.
  int64_t follow_idx;
  bool tie;
  std::tie(follow_idx, tie) =
      ChooseOperandToFollow(strategy_map, depth_map, alias_map, max_depth, ins);

  if (!tie || AllowTieFollowing(ins)) {
    strategy_group->following = strategy_map.at(ins->operand(follow_idx)).get();
  } else {
    strategy_group->following = nullptr;
  }

  // Get all possible sharding specs from operands.
  for (int64_t i = 0; i < ins->operand_count(); ++i) {
    if (strategy_group->following != nullptr && i != follow_idx) {
      // If ins follows one operand, do not consider sharding specs from
      // other operands.
      continue;
    }

    StrategyGroup* src_strategy_group = strategy_map.at(ins->operand(i)).get();
    CHECK(!src_strategy_group->is_tuple);

    FollowArrayOrTokenStrategyGroup(*src_strategy_group, ins->shape(),
                                    instruction_id, cluster_env,
                                    pretrimmed_strategy_map, *strategy_group);
  }

  if (ins->opcode() == HloOpcode::kAdd) {
    // Adjust the resharding costs for AllReduceReassociate pass.
    // The AllReduceReassociate pass can simplify
    // allreduce(x) + allreduce(y) to allreduce(x + y),
    // so we adjust the resharding costs to reflect this optimization.

    // TODO(zhuohan): The current implementation only works for
    // x = a + b. We also need to cover cases where there are
    // more than two operands (i.e., x = a + b + c).
    if (ins->operand(0)->opcode() == HloOpcode::kDot &&
        ins->operand(1)->opcode() == HloOpcode::kDot) {
      associative_dot_pairs.push_back({strategy_map.at(ins->operand(0)).get(),
                                       strategy_map.at(ins->operand(1)).get()});
    }
  }
  return strategy_group;
}

// Generates strategies for instructions in manually sharded sub-graphs. The
// generated strategies are present only as a way to take the memory consumption
// of such instructions into account (hence they have all costs expect memory
// costs set to zero). While the generated strategies have a replicated
// output_sharding, we skip these instructions when setting sharding
// annotations, so the output_sharding essentially remains unused.
std::unique_ptr<StrategyGroup> HandleManuallyShardedInstruction(
    const HloInstruction* ins, const Shape& shape, const size_t instruction_id,
    StrategyGroups& strategy_groups, StrategyMap& strategy_map) {
  std::unique_ptr<StrategyGroup> strategy_group;
  if (shape.IsTuple()) {
    strategy_group = CreateTupleStrategyGroup(instruction_id);
    strategy_group->childs.reserve(shape.tuple_shapes_size());
    for (size_t i = 0; i < shape.tuple_shapes_size(); ++i) {
      std::unique_ptr<StrategyGroup> child_strategies =
          HandleManuallyShardedInstruction(ins, shape.tuple_shapes(i),
                                           instruction_id, strategy_groups,
                                           strategy_map);
      child_strategies->tuple_element_idx = i;
      strategy_group->childs.push_back(std::move(child_strategies));
    }
  } else if (shape.IsToken() || shape.IsArray()) {
    strategy_group = CreateLeafStrategyGroup(instruction_id, ins, strategy_map,
                                             strategy_groups);
    ReshardingCosts communication_resharding_costs;
    ReshardingCosts memory_resharding_costs;
    std::vector<std::optional<HloSharding>> input_shardings;

    if (ins->operand_count() > 0 && ins->operand(0)->shape().IsTuple()) {
      CHECK_EQ(ins->operand_count(), 1)
          << "Do not support instructions with more than one tuple "
             "operand. If this CHECK fails, we will need to fix "
             "b/233412625.";
      std::tie(communication_resharding_costs, memory_resharding_costs,
               input_shardings) =
          ReshardingCostsForTupleOperand(
              ins->operand(0), strategy_map.at(ins->operand(0)).get());
    } else {
      for (int64_t k = 0; k < ins->operand_count(); ++k) {
        const HloInstruction* operand = ins->operand(k);
        communication_resharding_costs.push_back(std::vector<double>(
            strategy_map.at(operand)->strategies.size(), 0));
        memory_resharding_costs.push_back(std::vector<double>(
            strategy_map.at(operand)->strategies.size(), 0));
      }
    }
    strategy_group->strategies.push_back(ShardingStrategy(
        {"MANUAL", HloSharding::Replicate(), 0, 0,
         static_cast<double>(ShapeUtil::ByteSizeOf(shape)),
         std::move(communication_resharding_costs),
         std::move(memory_resharding_costs), std::move(input_shardings)}));
  } else {
    LOG(FATAL) << "Unsupported instruction shape: " << shape.DebugString();
  }
  return strategy_group;
}

std::unique_ptr<StrategyGroup> CreateReshapeStrategies(
    const size_t instruction_id, const HloInstruction* ins,
    const StrategyMap& strategy_map, const ClusterEnvironment& cluster_env,
    const bool only_allow_divisible, const double replicated_penalty,
    const InstructionBatchDimMap& batch_dim_map,
    const AutoShardingOption& option, StrategyGroups& strategy_groups,
    const CallGraph& call_graph) {
  const Array<int64_t>& device_mesh = cluster_env.device_mesh_;

  int mesh_nn_dims = VectorGreaterThanOneElementCount(device_mesh.dimensions());
  std::unique_ptr<StrategyGroup> strategy_group = CreateLeafStrategyGroup(
      instruction_id, ins, strategy_map, strategy_groups);

  if (mesh_nn_dims < 2 || !option.allow_mixed_mesh_shape) {
    const HloInstruction* operand = ins->operand(0);

    // Create follow strategies
    const StrategyGroup* src_strategy_group = strategy_map.at(operand).get();
    CHECK(!src_strategy_group->is_tuple);
    strategy_group->following = src_strategy_group;

    for (int64_t sid = 0; sid < src_strategy_group->strategies.size(); ++sid) {
      std::optional<HloSharding> output_spec =
          hlo_sharding_util::ReshapeSharding(
              operand->shape(), ins->shape(),
              src_strategy_group->strategies[sid].output_sharding);

      if (!output_spec.has_value()) {
        continue;
      }

      if (!IsValidTileAssignment(*output_spec)) {
        continue;
      }

      if (!TileAssignmentMatchesMesh(*output_spec, device_mesh)) {
        continue;
      }
      const std::string name = ToStringSimple(*output_spec);
      double compute_cost = 0, communication_cost = 0;
      double memory_cost =
          ByteSizeOfShapeWithSharding(ins->shape(), output_spec);
      std::vector<double> communication_resharding_costs =
          CommunicationReshardingCostVector(
              src_strategy_group, operand->shape(),
              src_strategy_group->strategies[sid].output_sharding, cluster_env);
      std::vector<double> memory_resharding_costs = MemoryReshardingCostVector(
          src_strategy_group, operand->shape(),
          src_strategy_group->strategies[sid].output_sharding, cluster_env);
      strategy_group->strategies.push_back(ShardingStrategy(
          {name,
           *output_spec,
           compute_cost,
           communication_cost,
           memory_cost,
           {communication_resharding_costs},
           {memory_resharding_costs},
           {src_strategy_group->strategies[sid].output_sharding}}));
    }
  }

  if (strategy_group->strategies.empty()) {
    // Fail to create follow strategies, enumerate all possible cases
    VLOG(2) << "Enumerating all strategies for reshape";
    FillAllStrategiesForArray(
        strategy_group, ins, ins->shape(), cluster_env, strategy_map, option,
        replicated_penalty, batch_dim_map, call_graph, only_allow_divisible,
        /* create_replicated_strategies */ true,
        /* create_partially_replicated_strategies */ true);
  }

  return strategy_group;
}

AutoShardingSolverResult CallSolver(
    const HloModule& hlo_module, const HloLiveRange& hlo_live_range,
    const StrategyMap& strategy_map, const StrategyGroups& strategy_groups,
    const CostGraph& cost_graph, const AliasSet& alias_set,
    const std::vector<std::pair<LivenessIdx, LivenessIdx>>& node_intervals,
    const std::vector<std::pair<LivenessIdx, LivenessIdx>>& edge_intervals,
    const std::vector<absl::btree_set<int64_t>>& node_groups,
    const std::vector<absl::btree_set<int64_t>>& edge_groups,
    const std::vector<NodeStrategyIdx>& s_hint, const bool compute_iis,
    const int64_t solver_timeout_in_seconds, const AutoShardingOption& option,
    std::optional<double> max_cost, absl::string_view request_name,
    const absl::flat_hash_map<std::string, const HloInstruction*>&
        sharding_propagation_solution,
    bool deterministic_mode) {
  // Serialize edges and edge costs to 1d numpy arrays.
  AutoShardingSolverRequest request;
  request.set_module_name(hlo_module.name());
  request.set_num_nodes(strategy_groups.size());
  request.set_memory_budget(option.memory_budget_per_device);
  request.mutable_s_len()->Add(cost_graph.node_lens_.begin(),
                               cost_graph.node_lens_.end());
  request.mutable_s_follow()->Add(cost_graph.follow_idx_.begin(),
                                  cost_graph.follow_idx_.end());
  request.mutable_s_hint()->Add(s_hint.begin(), s_hint.end());
  request.mutable_solver_timeout()->set_solver_timeout_in_seconds(
      solver_timeout_in_seconds);
  // Only apply soft memory constraints if the overbudget coeff is nonnegative.
  if (option.memory_overbudget_coeff >= 0.0) {
    request.mutable_overbudget_coeff()->set_coeff(
        option.memory_overbudget_coeff);
  }
  request.set_crash_at_infinity_costs_check(!option.try_multiple_mesh_shapes);
  request.set_compute_iis(compute_iis);
  request.set_saltiplier(kSaltiplier);
  request.set_deterministic_mode(deterministic_mode);
  request.set_request_name(std::string(request_name));
  request.set_enable_memory_edge_costs(option.model_resharding_memory_costs);
  // If we're removing user shardings, we are probably doing internal testing /
  // debugging where additional output from the solver might be helpful.
  request.set_enable_output(
      option.preserve_shardings ==
      AutoShardingOption::PreserveShardingsType::kRemoveAllShardings);
  if (max_cost) {
    request.mutable_max_cost()->set_coeff(*max_cost);
  }
  for (const auto& [edge, edge_cost] : cost_graph.edge_costs_) {
    AutoShardingSolverRequest_Pair raw_edge;
    raw_edge.set_first(edge.first);
    raw_edge.set_second(edge.second);
    *request.add_edges() = raw_edge;
    AutoShardingSolverRequest_Costs rij;
    AutoShardingSolverRequest_Costs mij;
    for (NodeStrategyIdx i = 0; i < edge_cost.n_; i++) {
      for (NodeStrategyIdx j = 0; j < edge_cost.m_; j++) {
        rij.add_costs(edge_cost(i, j).communication_cost);
        mij.add_costs(edge_cost(i, j).memory_cost);
      }
    }
    request.mutable_resharding_costs()->Add(std::move(rij));
    request.mutable_memory_edge_costs()->Add(std::move(mij));
  }

  const HloInstructionSequence& sequence =
      hlo_live_range.flattened_instruction_sequence();
  const std::vector<HloInstruction*>& instructions = sequence.instructions();

  // Serialize node costs.
  int num_nodes_without_default = 0;
  for (NodeIdx node_idx = 0; node_idx < request.num_nodes(); ++node_idx) {
    const StrategyGroup* strategy_group = strategy_groups[node_idx];
    const auto instruction = instructions.at(strategy_group->instruction_id);
    const auto instruction_name = instruction->name();
    const auto opcode = HloOpcodeString(instruction->opcode());
    request.add_instruction_names(
        absl::StrCat(instruction_name, " (id: ", node_idx, ")"));
    request.add_opcodes(std::string(opcode));
    AutoShardingSolverRequest_Costs ci, di, mi, pi;
    AutoShardingSolverRequest_Names strategy_names;
    std::optional<HloSharding> default_strategy;
    auto iter = sharding_propagation_solution.find(instruction_name);
    if (iter != sharding_propagation_solution.end()) {
      CHECK(iter->second->has_sharding()) << iter->second->ToString();
      default_strategy = iter->second->sharding();
      if (strategy_group->tuple_element_idx) {
        const auto& tuple_elements = iter->second->sharding().tuple_elements();
        CHECK_LT(*strategy_group->tuple_element_idx, tuple_elements.size());
        default_strategy =
            tuple_elements.at(*strategy_group->tuple_element_idx);
      }
    }
    for (NodeStrategyIdx j = 0; j < strategy_group->strategies.size(); ++j) {
      const ShardingStrategy& strategy = strategy_group->strategies[j];
      const HloSharding& sharding = strategy.output_sharding;
      ci.add_costs(strategy.compute_cost);
      di.add_costs(strategy.communication_cost +
                   cost_graph.extra_node_costs_[node_idx][j]);
      mi.add_costs(strategy.memory_cost);
      pi.add_costs(default_strategy && sharding == *default_strategy ? 0 : 1);
      strategy_names.add_names(sharding.ToString());
    }
    if (option.use_sharding_propagation_for_default_shardings &&
        *std::min_element(pi.costs().begin(), pi.costs().end()) > 0) {
      LOG(WARNING) << "No default strategy for {node_idx " << node_idx
                   << ", instruction ID " << strategy_group->instruction_id
                   << ", instruction name " << instruction_name << "}";
      ++num_nodes_without_default;
    }
    request.mutable_computation_costs()->Add(std::move(ci));
    request.mutable_communication_costs()->Add(std::move(di));
    request.mutable_memory_costs()->Add(std::move(mi));
    request.mutable_departure_costs()->Add(std::move(pi));
    request.mutable_strategy_names()->Add(std::move(strategy_names));
  }
  LOG(INFO) << "Total nodes without default: " << num_nodes_without_default;

  // Serialize special edges that forces a alias pair have the same sharding
  // spec.
  std::vector<std::pair<NodeIdx, NodeIdx>> new_followers;
  for (const auto& pair : alias_set) {
    const StrategyGroup* src_strategy_group = strategy_groups[pair.first];
    const StrategyGroup* dst_strategy_group = strategy_groups[pair.second];
    Matrix<double> raw_cost(src_strategy_group->strategies.size(),
                            dst_strategy_group->strategies.size());
    for (NodeStrategyIdx i = 0; i < src_strategy_group->strategies.size();
         ++i) {
      for (NodeStrategyIdx j = 0; j < dst_strategy_group->strategies.size();
           ++j) {
        if (src_strategy_group->strategies[i].output_sharding ==
            dst_strategy_group->strategies[j].output_sharding) {
          raw_cost(i, j) = 0.0;
        } else {
          raw_cost(i, j) = 1.0;
        }
      }
    }
    NodeIdx idx_a = pair.first;
    NodeIdx idx_b = pair.second;
    std::vector<NodeStrategyIdx> row_indices;
    std::vector<NodeStrategyIdx> col_indices;

    if (request.s_follow(idx_a) >= 0) {
      row_indices = cost_graph.reindexing_vector_.at(idx_a);
      idx_a = request.s_follow(idx_a);
    } else {
      row_indices.assign(request.s_len(idx_a), 0);
      std::iota(row_indices.begin(), row_indices.end(), 0);
    }

    if (request.s_follow(idx_b) >= 0) {
      col_indices = cost_graph.reindexing_vector_.at(idx_b);
      idx_b = request.s_follow(idx_b);
    } else {
      col_indices.assign(request.s_len(idx_b), 0);
      std::iota(col_indices.begin(), col_indices.end(), 0);
    }

    CHECK_EQ(request.s_len(idx_a), row_indices.size());
    CHECK_EQ(request.s_len(idx_b), col_indices.size());

    AutoShardingSolverRequest_Costs vij;
    for (NodeStrategyIdx i : row_indices) {
      for (NodeStrategyIdx j : col_indices) {
        vij.add_costs(raw_cost(i, j));
      }
    }
    bool convertible = (row_indices.size() == col_indices.size());
    for (NodeStrategyIdx i = 0; i < row_indices.size() && convertible; ++i) {
      if (vij.costs(i * col_indices.size() + i) != 0.0) convertible = false;
    }
    if (convertible && option.allow_alias_to_follower_conversion) {
      new_followers.push_back({idx_a, idx_b});
    } else {
      AutoShardingSolverRequest_Pair alias;
      alias.set_first(idx_a);
      alias.set_second(idx_b);
      *request.add_aliases() = alias;
      request.mutable_value_costs()->Add(std::move(vij));
    }
  }

  // Process any new followers that had originally been modeled as aliases.
  auto s_follow = request.mutable_s_follow();
  for (auto [follower, followee] : new_followers) {
    // New followers may have introduced chains, so find the root nodes.
    while (s_follow->at(follower) >= 0) follower = s_follow->at(follower);
    while (s_follow->at(followee) >= 0) followee = s_follow->at(followee);
    if (follower != followee) s_follow->Set(follower, followee);
  }

  // Flatten the follower indices to remove any transitive arcs.
  for (NodeIdx node_idx = 0; node_idx < request.num_nodes(); ++node_idx) {
    if (s_follow->at(node_idx) < 0) continue;
    while (s_follow->at(s_follow->at(node_idx)) >= 0) {
      s_follow->Set(node_idx, s_follow->at(s_follow->at(node_idx)));
    }
  }

  for (const auto& interval : node_intervals) {
    AutoShardingSolverRequest_Pair pair;
    pair.set_first(interval.first);
    pair.set_second(interval.second);
    *request.add_node_intervals() = std::move(pair);
  }
  for (const auto& interval : edge_intervals) {
    AutoShardingSolverRequest_Pair pair;
    pair.set_first(interval.first);
    pair.set_second(interval.second);
    *request.add_edge_intervals() = std::move(pair);
  }
  for (const auto& reduced_group : node_groups) {
    AutoShardingSolverRequest_Group group;
    group.mutable_prims()->Add(reduced_group.begin(), reduced_group.end());
    *request.add_node_groups() = std::move(group);
  }
  for (const auto& reduced_group : edge_groups) {
    AutoShardingSolverRequest_Group group;
    group.mutable_prims()->Add(reduced_group.begin(), reduced_group.end());
    *request.add_edge_groups() = std::move(group);
  }

  PopulateTemporalValues(cost_graph, request);

  return CallORToolsSolver(request);
}

void CheckHloSharding(
    const HloInstructionSequence& sequence,
    const absl::flat_hash_set<const HloInstruction*>& instructions_to_shard,
    const size_t total_num_devices) {
  const std::vector<HloInstruction*>& instructions = sequence.instructions();
  std::vector<std::pair<size_t, std::string>> size_string;
  for (const HloInstruction* ins : instructions) {
    if (!instructions_to_shard.contains(ins) || !ins->has_sharding()) {
      continue;
    }
    if (!ins->shape().IsTuple() &&
        ins->opcode() != HloOpcode::kGetTupleElement) {
      // TODO(yuemmawang) Check other cases when it's helpful (it's not
      // needed so far).
      double size = ByteSizeOfShape(ins->shape()) / 1024 / 1024 / 1024;
      if ((!ShardingIsComplete(ins->sharding(), total_num_devices) ||
           ins->sharding().IsReplicated()) &&
          size > 1) {
        LOG(INFO) << "Instruction is not fully sharded: (" << size << " GB) "
                  << ins->ToString();
      } else if (!ins->has_sharding()) {
        LOG(INFO) << "Instruction does not have sharding: " << ins->name();
      }
      for (const auto& op : ins->operands()) {
        if (op->has_sharding()) {
          if (op->sharding().IsReplicated() || ins->sharding().IsReplicated()) {
            continue;
          }
          const std::vector<int64_t> ins_sharded_dims =
              VectorGreaterThanOneElementIndices(
                  ins->sharding().tile_assignment().dimensions(),
                  ins->sharding().ReplicateOnLastTileDim());
          const std::vector<int64_t> op_sharded_dims =
              VectorGreaterThanOneElementIndices(
                  op->sharding().tile_assignment().dimensions(),
                  op->sharding().ReplicateOnLastTileDim());
          bool not_consistent = false;
          if (ins_sharded_dims.size() != op_sharded_dims.size()) {
            not_consistent = true;
          } else {
            for (size_t i = 0; i < ins_sharded_dims.size(); i++) {
              if (op->shape().dimensions().at(op_sharded_dims.at(i)) !=
                  ins->shape().dimensions().at(ins_sharded_dims.at(i))) {
                not_consistent = true;
              }
            }
          }
          if (not_consistent) {
            // Prints the inconsistent shardings, which may indicate causes
            // of resharding overheads, and some inconsistent shardings are
            // unavoidable.
            size_t op_size =
                ByteSizeOfShape(op->shape()) / (1024.0 * 1024 * 1024);
            std::string str = absl::StrCat("Shardings not consistent (op size ",
                                           op_size, " GB):", ins->ToString(),
                                           "\n Operand: ", op->ToString());
            size_string.push_back({op_size, std::move(str)});
          }
        } else {
          LOG(INFO) << "Instruction " << op->name()
                    << " does not have sharding.";
        }
      }
    }
  }
  struct {
    bool operator()(const std::pair<size_t, std::string>& a,
                    const std::pair<size_t, std::string>& b) const {
      return a.first > b.first;
    }
  } MemLarger;
  std::sort(size_string.begin(), size_string.end(), MemLarger);
  size_t k = 10;
  k = std::min(k, size_string.size());
  for (size_t t = 0; t < k; ++t) {
    LOG(INFO) << size_string.at(t).second;
  }
}

// Set the HloSharding for all instructions according to the ILP solution.
void SetHloSharding(
    const HloInstructionSequence& sequence,
    const absl::flat_hash_set<const HloInstruction*>& instructions_to_shard,
    const StrategyMap& strategy_map, const CostGraph& cost_graph,
    absl::Span<const NodeStrategyIdx> s_val, const bool last_iteration) {
  // Set the HloSharding for every instruction
  const std::vector<HloInstruction*>& instructions = sequence.instructions();

  for (HloInstruction* inst : instructions) {
    if (!instructions_to_shard.contains(inst)) {
      continue;
    }
    if (inst->opcode() == HloOpcode::kOutfeed ||
        inst->opcode() == HloOpcode::kRecv ||
        inst->opcode() == HloOpcode::kRecvDone ||
        inst->opcode() == HloOpcode::kSend ||
        inst->opcode() == HloOpcode::kSendDone) {
      continue;
    }
    auto iter = strategy_map.find(inst);
    if (iter == strategy_map.end()) {
      continue;
    }

    const StrategyGroup* strategy_group = iter->second.get();
    if (strategy_group->is_tuple) {
      const Shape& out_shape = inst->shape();
      ShapeTree<HloSharding> output_tuple_sharding(out_shape, Undefined());
      std::vector<HloSharding> output_flattened_shardings;

      std::function<void(const StrategyGroup*)> extract_tuple_shardings;
      bool set_tuple_sharding = true;

      extract_tuple_shardings = [&](const StrategyGroup* strategy_group) {
        if (strategy_group->is_tuple) {
          for (const auto& child_strategies : strategy_group->childs) {
            extract_tuple_shardings(child_strategies.get());
          }
        } else {
          NodeIdx node_idx = strategy_group->node_idx;
          NodeStrategyIdx stra_idx = s_val[node_idx];
          // Do not set completed sharding before the last iteration
          if (strategy_group->strategies[stra_idx]
                  .output_sharding.IsReplicated() &&
              !last_iteration) {
            set_tuple_sharding = false;
          }
          output_flattened_shardings.push_back(
              strategy_group->strategies[stra_idx].output_sharding);
        }
      };
      extract_tuple_shardings(strategy_group);

      // Create Tuple HloSharding.
      int i = 0;
      for (auto& leaf : output_tuple_sharding.leaves()) {
        leaf.second = output_flattened_shardings[i++];
      }
      if (set_tuple_sharding) {
        inst->set_sharding(HloSharding::Tuple(output_tuple_sharding));
      }
    } else {
      const HloSharding& sharding_spec =
          GetShardingStrategy(inst, strategy_map, cost_graph, s_val)
              .output_sharding;
      if (IsUndefined(sharding_spec)) {
        continue;
      }
      // Do not overwrite existing complete shardings.
      if (sharding_spec.IsReplicated() && !last_iteration) {
        LOG(INFO) << "skip setting shardings for inst " << inst->name();
      } else {
        inst->set_sharding(sharding_spec);
      }
    }
  }
}

absl::Status SetHloShardingPostProcessing(
    const HloInstructionSequence& sequence,
    const absl::flat_hash_set<const HloInstruction*>& instructions_to_shard,
    const StrategyMap& strategy_map, const CostGraph& cost_graph,
    absl::Span<const NodeStrategyIdx> s_val,
    const ClusterEnvironment& cluster_env, const bool crash_at_error,
    absl::flat_hash_map<std::string, std::vector<HloSharding>>&
        preserve_shardings) {
  const std::vector<HloInstruction*>& instructions = sequence.instructions();
  const Array<int64_t>& device_mesh = cluster_env.device_mesh_;
  // Post process: fix some corner cases.
  ReshardingCache resharding_cache_entity;
  ReshardingCache* resharding_cache = &resharding_cache_entity;

  for (HloInstruction* inst : instructions) {
    if (!instructions_to_shard.contains(inst) ||
        spmd::IsSPMDShardToFullShapeCustomCall(inst)) {
      continue;
    }
    // For some dot instructions and resharding cases, our formulation thinks
    // they are valid. But the spmd partitioner cannot infer the correct
    // dot algorithms or resharding algorithm from the input/output sharding.
    // It then generates bad fallback code.
    // Here we insert some extra annotated identity instructions to help the
    // spmd partitioner generate correct code.

    if (inst->opcode() == HloOpcode::kDot ||
        inst->opcode() == HloOpcode::kConvolution) {
      const ShardingStrategy& stra =
          GetShardingStrategy(inst, strategy_map, cost_graph, s_val);
      const HloInstruction* lhs = inst->operand(0);
      const HloInstruction* rhs = inst->operand(1);
      const HloSharding& lhs_sharding = lhs->sharding();
      const HloSharding& rhs_sharding = rhs->sharding();
      std::vector<int64_t> lhs_con_dims;
      std::vector<int64_t> rhs_con_dims;
      if (inst->opcode() == HloOpcode::kDot) {
        const DotDimensionNumbers& dot_dnums = inst->dot_dimension_numbers();
        lhs_con_dims.push_back(dot_dnums.lhs_contracting_dimensions()[0]);
        rhs_con_dims.push_back(dot_dnums.rhs_contracting_dimensions()[0]);
      } else {
        const ConvolutionDimensionNumbers& conv_dnums =
            inst->convolution_dimension_numbers();
        lhs_con_dims.push_back(conv_dnums.input_feature_dimension());
        rhs_con_dims.push_back(conv_dnums.kernel_input_feature_dimension());
      }

      const std::vector<int64_t>& lhs_tensor_dim_to_mesh_dim =
          cluster_env.GetTensorDimToMeshDimWrapper(
              lhs->shape(), lhs_sharding,
              /* consider_reverse_device_meshes */ true,
              /* crash_at_error */ crash_at_error);
      const std::vector<int64_t>& rhs_tensor_dim_to_mesh_dim =
          cluster_env.GetTensorDimToMeshDimWrapper(
              rhs->shape(), rhs_sharding,
              /* consider_reverse_device_meshes */ true,
              /* crash_at_error */ crash_at_error);

      if (lhs_tensor_dim_to_mesh_dim.size() != lhs->shape().rank() ||
          rhs_tensor_dim_to_mesh_dim.size() != rhs->shape().rank()) {
        return absl::InvalidArgumentError(
            "Cannot generate tensor dim to mesh dim mapping");
      }

      if (absl::StrContains(stra.name, "allreduce") &&
          std::any_of(lhs_con_dims.begin(), lhs_con_dims.end(),
                      [&lhs_tensor_dim_to_mesh_dim](int64_t dim) {
                        return lhs_tensor_dim_to_mesh_dim[dim] == -1;
                      }) &&
          std::any_of(rhs_con_dims.begin(), rhs_con_dims.end(),
                      [&rhs_tensor_dim_to_mesh_dim](int64_t dim) {
                        return rhs_tensor_dim_to_mesh_dim[dim] == -1;
                      })) {
        // Allow duplicated dot computation in this case to reduce
        // communication
      } else {
        CHECK(stra.input_shardings.size() == 2)
            << "Dot op requires both operands to have input shardings, "
               "but get instruction: "
            << inst->ToString() << ", strategy : " << stra.ToString();
        if (stra.input_shardings[0].has_value()) {
          TF_RETURN_IF_ERROR(FixMixedMeshShapeResharding(
              inst, 0, stra.input_shardings[0].value(), device_mesh,
              resharding_cache));
        }
        if (stra.input_shardings[1].has_value()) {
          TF_RETURN_IF_ERROR(FixMixedMeshShapeResharding(
              inst, 1, stra.input_shardings[1].value(), device_mesh,
              resharding_cache));
        }
      }
    } else if (inst->opcode() == HloOpcode::kOutfeed ||
               inst->opcode() == HloOpcode::kSendDone) {
      // Outfeed: Outfeed operand shardings are handled in downstream passes and
      // so we ignore outfeed ops here. However, we need to ensure that outfeed
      // ops which have user shardings have their shardings restored at the
      // end. If not, this can lead to errors downstream in the spmd_partitioner
      // pass.

      // In the analysis itself, we use replicated strategies as a stand-in for
      // the (expected) maximal sharding annotations that send-done ops usually
      // have. Here we restore these maximal shardings if present.
      auto preserved_sharding_iter = preserve_shardings.find(inst->name());
      if (preserved_sharding_iter != preserve_shardings.end()) {
        const std::vector<HloSharding>& preserved_sharding =
            preserved_sharding_iter->second;
        if (preserved_sharding.size() > 1) {
          std::vector<Shape> tuple_elements_shape(
              inst->operand(0)->shape().tuple_shapes().begin(),
              inst->operand(0)->shape().tuple_shapes().end());
          tuple_elements_shape.push_back(inst->operand(1)->shape());
          Shape output_tuple_sharding_shape =
              ShapeUtil::MakeTupleShape(tuple_elements_shape);
          ShapeTree<HloSharding> output_tuple_sharding(
              output_tuple_sharding_shape, Undefined());
          size_t i = 0;
          for (std::pair<ShapeIndex, HloSharding>& leaf :
               output_tuple_sharding.leaves()) {
            leaf.second = preserved_sharding.at(i++);
          }
          inst->set_sharding(HloSharding::Tuple(output_tuple_sharding));
        } else {
          CHECK_EQ(preserved_sharding.size(), 1);  // Crash OK
          inst->set_sharding(preserved_sharding[0]);
        }
      }
      continue;
    } else if (inst->opcode() == HloOpcode::kSend ||
               inst->opcode() == HloOpcode::kRecv ||
               inst->opcode() == HloOpcode::kRecvDone) {
      // In the analysis itself, we use replicated strategies as a stand-in for
      // the (expected) maximal sharding annotations that send ops usually
      // have. Here we restore these maximal shardings if present.
      auto preserved_sharding_iter = preserve_shardings.find(inst->name());
      if (preserved_sharding_iter != preserve_shardings.end()) {
        const std::vector<HloSharding>& preserved_sharding =
            preserved_sharding_iter->second;
        if (preserved_sharding.size() > 1) {
          inst->set_sharding(
              HloSharding::Tuple(inst->shape(), preserved_sharding));
        } else {
          if (preserved_sharding.size() != 1) {
            return absl::InternalError(absl::StrCat(
                "An empty sharding was preserved for ", inst->name(),
                ". This should be reported as a bug."));
          }
          inst->set_sharding(preserved_sharding[0]);
        }
      }
      continue;
    } else {
      if (inst->shape().IsTuple()) {
        // While we do not support nested tuples fully (b/332951306), this is a
        // hack to get things to work in some cases (specifically observed for
        // the llama and gemma models) where nested tuples as used as
        // inputs/outputs of the kOptimizationBarrier instruction.
        if (absl::c_any_of(
                inst->shape().tuple_shapes(),
                [](const Shape& shape) { return shape.IsTuple(); })) {
          continue;
        }
        switch (inst->opcode()) {
          case HloOpcode::kReduce:
          case HloOpcode::kCustomCall:
          case HloOpcode::kRngBitGenerator:
          case HloOpcode::kSort: {
            for (size_t i = 0; i < inst->shape().tuple_shapes_size(); ++i) {
              const ShardingStrategy& stra =
                  GetShardingStrategyForTuple(inst, {static_cast<int64_t>(i)},
                                              strategy_map, cost_graph, s_val);
              if (stra.input_shardings.size() > i &&
                  stra.input_shardings[i].has_value()) {
                TF_RETURN_IF_ERROR(FixMixedMeshShapeResharding(
                    inst, i, stra.input_shardings[i].value(), device_mesh,
                    resharding_cache));
              }
            }
            break;
          }
          case HloOpcode::kTuple: {
            for (size_t i = 0; i < inst->shape().tuple_shapes_size(); ++i) {
              const ShardingStrategy& stra =
                  GetShardingStrategyForTuple(inst, {static_cast<int64_t>(i)},
                                              strategy_map, cost_graph, s_val);
              CHECK_EQ(stra.input_shardings.size(), 1);
              CHECK(stra.input_shardings[0].has_value());
              TF_RETURN_IF_ERROR(FixMixedMeshShapeResharding(
                  inst, i, stra.input_shardings[0].value(), device_mesh,
                  resharding_cache));
            }
            break;
          }
          case HloOpcode::kGetTupleElement: {
            std::vector<std::optional<HloSharding>> dst_shardings(
                inst->shape().tuple_shapes_size(), std::nullopt);
            for (size_t i = 0; i < inst->shape().tuple_shapes_size(); ++i) {
              CHECK(!inst->shape().tuple_shapes(i).IsTuple())
                  << "We currently do not support ops with nested tuples as "
                     "output. See b/332951306.";
              const ShardingStrategy& stra =
                  GetShardingStrategyForTuple(inst, {static_cast<int64_t>(i)},
                                              strategy_map, cost_graph, s_val);
              if (!stra.input_shardings.empty() &&
                  stra.input_shardings[0].has_value()) {
                dst_shardings[i] = stra.input_shardings[0].value();
              }
            }
            TF_RETURN_IF_ERROR(
                FixMixedMeshShapeReshardingGetTupleElementWithTupleOutput(
                    inst, dst_shardings, device_mesh));
            break;
          }

          case HloOpcode::kWhile:
          case HloOpcode::kInfeed:
          case HloOpcode::kOptimizationBarrier:
          case HloOpcode::kConditional:
          case HloOpcode::kParameter: {
            break;
          }
          default:
            LOG(FATAL) << "Unhandled instruction: " + inst->ToString();
        }
      } else {
        const ShardingStrategy& stra =
            GetShardingStrategy(inst, strategy_map, cost_graph, s_val);

        if (stra.input_shardings.empty()) {
          continue;
        }
        if (inst->opcode() == HloOpcode::kGetTupleElement) {
          TF_RETURN_IF_ERROR(FixMixedMeshShapeReshardingGetTupleElement(
              inst, inst->sharding(), device_mesh, preserve_shardings));
        } else {
          for (size_t i = 0; i < inst->operand_count(); ++i) {
            if (stra.input_shardings.size() > i &&
                stra.input_shardings[i].has_value()) {
              TF_RETURN_IF_ERROR(FixMixedMeshShapeResharding(
                  inst, i, stra.input_shardings[i].value(), device_mesh,
                  resharding_cache));
            }
          }
        }
      }
    }
  }
  return absl::OkStatus();
}

// Print liveness set for debugging.
std::string PrintLivenessSet(const LivenessSet& liveness_set) {
  std::string str("Liveness Set\n");
  for (LivenessIdx time_idx = 0; time_idx < liveness_set.size(); ++time_idx) {
    std::vector<std::string> names;
    names.reserve(liveness_set[time_idx].size());
    for (const HloValue* value : liveness_set[time_idx]) {
      names.push_back(absl::StrCat(value->instruction()->name(),
                                   value->index().ToString()));
    }
    std::sort(names.begin(), names.end());
    absl::StrAppend(&str, "Time ", time_idx, ": ", absl::StrJoin(names, ", "),
                    "\n");
  }
  return str;
}

// Print sorted instructions.
std::string PrintInstructions(const HloInstructionSequence& sequence) {
  std::string str;
  const std::vector<HloInstruction*>& instructions = sequence.instructions();
  for (size_t i = 0; i < instructions.size(); ++i) {
    absl::StrAppend(&str, "Instruction ", i, ": ", instructions[i]->ToString(),
                    "\n");
  }
  return str;
}

// Print strategy map for debugging.
std::string PrintStrategyMap(const StrategyMap& strategy_map,
                             const HloInstructionSequence& sequence) {
  std::string str("Strategy Map\n");
  const std::vector<HloInstruction*>& instructions = sequence.instructions();
  for (size_t i = 0; i < instructions.size(); ++i) {
    absl::StrAppend(&str, "Instruction ", i, ": ", instructions[i]->ToString(),
                    "\n", strategy_map.at(instructions[i])->ToString());
  }
  return str;
}

// Print the chosen auto sharding strategy for debugging.
// TODO (zhuohan): Update the following function.
std::string PrintAutoShardingSolution(const HloInstructionSequence& sequence,
                                      const LivenessSet& liveness_set,
                                      const StrategyMap& strategy_map,
                                      const StrategyGroups& strategy_groups,
                                      const CostGraph& cost_graph,
                                      absl::Span<const NodeStrategyIdx> s_val,
                                      const double objective) {
  std::string str("=== Auto sharding strategy ===\n");
  const std::vector<HloInstruction*>& instructions = sequence.instructions();
  size_t N = strategy_groups.size();

  // Print the chosen strategy
  for (NodeIdx node_idx = 0; node_idx < N; ++node_idx) {
    absl::StrAppend(
        &str, node_idx, " ",
        ToAdaptiveString(
            instructions[strategy_groups[node_idx]->instruction_id]),
        " ");
    NodeStrategyIdx stra_idx = cost_graph.RemapIndex(node_idx, s_val[node_idx]);
    if (cost_graph.follow_idx_[node_idx] < 0) {
      absl::StrAppend(
          &str, strategy_groups[node_idx]->strategies[stra_idx].ToString(),
          "\n");
    } else {
      absl::StrAppend(
          &str, strategy_groups[node_idx]->strategies[stra_idx].ToString(),
          " follow ", cost_graph.follow_idx_[node_idx], "\n");
    }
  }

  return str;
}

std::string PrintSolutionMemoryUsage(const LivenessSet& liveness_set,
                                     const StrategyMap& strategy_map,
                                     const CostGraph& cost_graph,
                                     absl::Span<const NodeStrategyIdx> s_val) {
  // Print the memory usage
  std::string str("=== Memory ===\n");
  std::vector<std::pair<LivenessIdx, double>> time_memory_usage;
  // Function that gets the memory usage of a StrategyGroup belongs to one
  // tensor.
  std::function<double(const StrategyGroup*)> calculate_memory_usage;
  calculate_memory_usage = [&](const StrategyGroup* strategy_group) {
    if (strategy_group->is_tuple) {
      double m = 0.0;
      for (const auto& child : strategy_group->childs) {
        m += calculate_memory_usage(child.get());
      }
      return m;
    }
    NodeIdx ins_idx = strategy_group->node_idx;
    NodeStrategyIdx stra_idx = cost_graph.RemapIndex(ins_idx, s_val[ins_idx]);
    const ShardingStrategy& strategy = strategy_group->strategies[stra_idx];
    return strategy.memory_cost;
  };
  for (LivenessIdx time_idx = 0; time_idx < liveness_set.size(); ++time_idx) {
    double mem = 0.0;
    for (const auto& val : liveness_set.at(time_idx)) {
      const HloInstruction* ins = val->instruction();
      auto tmp = calculate_memory_usage(strategy_map.at(ins).get());
      mem += tmp;

      if (VLOG_IS_ON(6) && tmp / (1024 * 1024) > 1) {
        // Prints out the largest tensors.
        absl::StrAppend(&str, "  ", ins->name(),
                        ": mem += ", tmp / (1024 * 1024),
                        " MB; mem=", mem / (1024 * 1024), " MB\n");
      }
    }
    time_memory_usage.push_back({time_idx, mem});
    if (VLOG_IS_ON(6)) {
      absl::StrAppend(&str, "Time ", time_idx, ": ", mem / (1024 * 1024),
                      " MB\n");
    }
  }

  struct {
    bool operator()(std::pair<LivenessIdx, double> a,
                    std::pair<LivenessIdx, double> b) const {
      return a.second > b.second;
    }
  } TimeMemLarger;
  std::sort(time_memory_usage.begin(), time_memory_usage.end(), TimeMemLarger);

  absl::StrAppend(&str,
                  "Using memory costs from ShardingStrategy, the max memory "
                  "consumption is ",
                  time_memory_usage.front().second / (1024 * 1024 * 1024),
                  " GB at time ", time_memory_usage.front().first, "\n");

  // Gets largest tensors in top k time steps.
  size_t k = 3;
  k = std::min(k, time_memory_usage.size());
  std::vector<std::pair<std::string, double>> instruction_mem;
  for (LivenessIdx time_idx = 0; time_idx < k; time_idx++) {
    for (const auto& val : liveness_set[time_memory_usage.at(time_idx).first]) {
      const HloInstruction* ins = val->instruction();
      auto mem = calculate_memory_usage(strategy_map.at(ins).get());
      if (mem > 100 * 1024 * 1024) {
        instruction_mem.push_back(
            {absl::StrCat(ins->name(), val->index().ToString()), mem});
      }
    }
  }

  struct {
    bool operator()(std::pair<std::string, double> a,
                    std::pair<std::string, double> b) const {
      return a.second > b.second;
    }
  } NameMemLarger;
  std::sort(instruction_mem.begin(), instruction_mem.end(), NameMemLarger);

  size_t top_tensors = 10;
  top_tensors = std::min(top_tensors, instruction_mem.size());
  absl::StrAppend(&str, "Top ", top_tensors, " largest tensors:\n");
  for (size_t i = 0; i < top_tensors; i++) {
    absl::StrAppend(
        &str, "instruction name: ", instruction_mem.at(i).first,
        " memory usage: ", instruction_mem.at(i).second / (1024 * 1024 * 1024),
        "GB\n");
  }

  return str;
}

void SaveShardingForInstruction(
    const HloInstruction* inst, bool save_for_copy_users,
    absl::flat_hash_map<std::string, std::vector<HloSharding>>&
        preserve_shardings) {
  auto save_sharding = [&preserve_shardings](const HloInstruction* inst) {
    if (!inst->has_sharding()) {
      return;
    }
    if (!inst->sharding().IsTuple()) {
      preserve_shardings[inst->name()] = {inst->sharding()};
    } else {
      preserve_shardings[inst->name()] = inst->sharding().tuple_elements();
    }
  };

  save_sharding(inst);

  if (save_for_copy_users) {
    for (const auto user : inst->users()) {
      // Also preserve the shardings of copy ops that are the users of those
      // instructions.
      if (user->opcode() == HloOpcode::kCopy) {
        save_sharding(user);
      }
    }
  }
}

// Check whether the shardings that need to be preserved are preserved.
void CheckUserShardingPreservation(
    HloModule* module,
    const absl::flat_hash_map<std::string, std::vector<HloSharding>>&
        preserve_shardings) {
  for (const auto computation : module->computations()) {
    for (const auto inst : computation->instructions()) {
      if (preserve_shardings.find(inst->name()) == preserve_shardings.end()) {
        continue;
      }
      if (!inst->has_sharding()) {
        LOG(FATAL) << "User sharding is not preserved! Instruction with name "
                   << inst->name() << " should be: "
                   << preserve_shardings.at(inst->name())[0].ToString()
                   << "\nbut it's empty.";
      } else if (!inst->sharding().IsTuple() &&
                 !preserve_shardings.at(inst->name())[0].IsUnknown() &&
                 preserve_shardings.at(inst->name())[0] != inst->sharding()) {
        LOG(FATAL) << "User sharding is not preserved! Instruction with name "
                   << inst->name() << " should be: "
                   << preserve_shardings.at(inst->name())[0].ToString()
                   << "\nbut it's: " << inst->sharding().ToString();
      } else if (inst->sharding().IsTuple()) {
        const std::vector<HloSharding>* preserve_shardings_tuple =
            &preserve_shardings.at(inst->name());
        for (size_t i = 0; i < inst->shape().tuple_shapes_size(); i++) {
          if (!preserve_shardings_tuple->at(i).IsUnknown() &&
              preserve_shardings_tuple->at(i) !=
                  inst->sharding().tuple_elements().at(i)) {
            LOG(FATAL) << "Tuple sharding is not preserved! Instruction "
                          "with name "
                       << inst->name() << " " << i << "th tuple element "
                       << " should be: "
                       << preserve_shardings_tuple->at(i).ToString()
                       << "\nbut it's: "
                       << inst->sharding().tuple_elements().at(i).ToString();
          }
        }
      }
    }
  }
}

int64_t MemoryBudgetLowerBound(
    const HloModule& module,
    const absl::flat_hash_set<const HloInstruction*>& instructions_to_shard,
    const LivenessSet& liveness_set, const HloAliasAnalysis& alias_analysis,
    const int64_t num_devices,
    const absl::flat_hash_map<std::string, std::vector<HloSharding>>&
        preserved_shardings) {
  auto get_value_sharding = [](const HloValue* value) -> HloSharding {
    return !value->index().empty()
               ? value->instruction()->sharding().GetSubSharding(
                     value->instruction()->shape(), value->index())
               : value->instruction()->sharding();
  };

  // We below, that is HloValues A and B alias, and A has a sharding specified,
  // the same sharding is also used to compute the per-device memory
  // requirements of B. This can be done by associating shardings with buffers
  // as aliasing HloValues are mapped to the same buffer.
  absl::flat_hash_map<HloBuffer::Id, const HloValue*>
      buffer_to_sharded_value_mapping;
  bool vlog_is_on_5 = VLOG_IS_ON(5);
  for (const HloBuffer& buffer : alias_analysis.buffers()) {
    for (const HloValue* value : buffer.values()) {
      if (value->instruction()->has_sharding()) {
        if (vlog_is_on_5) {
          const HloSharding& this_value_sharding = get_value_sharding(value);
          auto iter = buffer_to_sharded_value_mapping.find(buffer.id());
          if (iter != buffer_to_sharded_value_mapping.end()) {
            const HloSharding& buffer_value_sharding =
                get_value_sharding(iter->second);
            if (this_value_sharding != buffer_value_sharding) {
              // TODO(pratikf): This is an unavoidable situation, but possibly
              // there is a better design decision that can be made here.
              VLOG(1)
                  << "We have a situation where two HloValues alias, but "
                     "they have different shardings. This can happen in the "
                     "presence of user-specified shardings, and is expected. "
                     "This, however, means that the memory budget estimate "
                     "is not very accurate. The aliasing HLOs are "
                  << value->ToShortString() << " and "
                  << iter->second->ToShortString();
            }
          }
        }
        buffer_to_sharded_value_mapping[buffer.id()] = value;
      }
    }
  }

  int64_t max_memory_usage = 0;
  absl::flat_hash_map<const HloValue*, int64_t> value_to_memory_size_mapping;
  for (LivenessIdx time_idx = 0; time_idx < liveness_set.size(); ++time_idx) {
    int64_t memory_usage = 0;
    for (const HloValue* value : liveness_set[time_idx]) {
      if (value->instruction()->shape().IsTuple() && value->index().empty()) {
        continue;
      }

      if (!instructions_to_shard.contains(value->instruction())) {
        memory_usage += ShapeUtil::ByteSizeOf(value->shape());
        continue;
      }

      auto iter1 = value_to_memory_size_mapping.find(value);
      if (iter1 != value_to_memory_size_mapping.end()) {
        memory_usage += iter1->second;
        continue;
      }

      std::optional<HloSharding> optional_sharding = std::nullopt;
      const HloBuffer& buffer = alias_analysis.GetBufferContainingValue(*value);
      auto iter2 = buffer_to_sharded_value_mapping.find(buffer.id());
      if (iter2 != buffer_to_sharded_value_mapping.end()) {
        // The instructions here can have partial sharding annotations from
        // previous iterations with partial mesh shapes when
        // solve_nd_sharding_iteratively is true. To exclude these, we only
        // utilize those shardings which corresponding to the current device
        // mesh.
        if (preserved_shardings.find(value->instruction()->name()) !=
            preserved_shardings.end()) {
          optional_sharding = get_value_sharding(iter2->second);
        } else {
          const HloSharding& value_sharding = get_value_sharding(iter2->second);
          if (!value_sharding.IsTiled() ||
              value_sharding.TotalNumTiles() == num_devices) {
            optional_sharding = value_sharding;
          }
        }
      }

      const Shape& shape =
          ShapeUtil::GetSubshape(value->instruction()->shape(), value->index());
      int64_t value_memory_usage = ByteSizeOfShapeIfShardedAcrossDevices(
          shape, num_devices, optional_sharding);
      value_to_memory_size_mapping[value] = value_memory_usage;
      memory_usage += value_memory_usage;
    }
    max_memory_usage = std::max(max_memory_usage, memory_usage);
  }

  return max_memory_usage;
}

void RecoverShardingsFromPartialMesh(
    const HloInstructionSequence& sequence,
    const absl::flat_hash_map<std::string, std::vector<HloSharding>>&
        preserve_shardings) {
  const std::vector<HloInstruction*>& instructions = sequence.instructions();

  for (HloInstruction* ins : instructions) {
    auto preserved_sharding_iter = preserve_shardings.find(ins->name());
    if (preserved_sharding_iter != preserve_shardings.end()) {
      const auto& preserved_sharding = preserved_sharding_iter->second;

      if (ins->shape().IsTuple() || (ins->opcode() == HloOpcode::kOutfeed &&
                                     preserved_sharding.size() > 1)) {
        Shape output_tuple_sharding_shape = ins->shape();
        if (ins->opcode() == HloOpcode::kOutfeed) {
          std::vector<Shape> tuple_elements_shape(
              ins->operand(0)->shape().tuple_shapes().begin(),
              ins->operand(0)->shape().tuple_shapes().end());
          tuple_elements_shape.push_back(ins->operand(1)->shape());
          output_tuple_sharding_shape =
              ShapeUtil::MakeTupleShape(tuple_elements_shape);
        }
        ShapeTree<HloSharding> output_tuple_sharding(
            output_tuple_sharding_shape, Undefined());
        size_t i = 0;
        for (auto& leaf : output_tuple_sharding.leaves()) {
          leaf.second = preserved_sharding.at(i++);
        }
        ins->set_sharding(HloSharding::Tuple(output_tuple_sharding));
      } else {
        ins->set_sharding(preserved_sharding.at(0));
      }
    }
  }
}

// DFS to find the replicated set starting from cur instruction.
void FindReplicateSet(
    HloInstruction* cur, const AliasMap& alias_map, const CostGraph& cost_graph,
    absl::Span<const NodeStrategyIdx> s_val, const StrategyMap& strategy_map,
    const ShardingStrategy& strategy, const HloInstruction* output,
    const bool do_all_gather_after_backward, HloInstruction*& transpose_inst,
    StableHashSet<HloInstruction*>& replicated_set,
    StableHashSet<HloInstruction*>& boundary_set,
    StableHashSet<HloInstruction*>& consumer_set,
    StableHashSet<const HloInstruction*>& visited) {
  visited.insert(cur);

  // Check whether the node is a boundary node.
  StableHashSet<HloInstruction*> users = UsersWithAlias(cur, alias_map, output);
  for (HloInstruction* consumer : users) {
    const HloInstruction* shape_inst = cur;

    // Allow at most one transpose.
    if (consumer->opcode() == HloOpcode::kTranspose &&
        (transpose_inst == nullptr ||
         DimensionsEqual(transpose_inst->shape(), consumer->shape()))) {
      shape_inst = consumer;
      transpose_inst = consumer;
      // TODO(zhuohan): Fix output_sharding comparison.
    }

    if (consumer->opcode() == HloOpcode::kTuple ||
        (do_all_gather_after_backward && IsParameterConvert(consumer)) ||
        GetShardingStrategy(consumer, strategy_map, cost_graph, s_val)
                .output_sharding != strategy.output_sharding ||
        !DimensionsEqual(consumer->shape(), shape_inst->shape())) {
      boundary_set.insert(cur);
      return;
    }
  }

  // If this node is not a boundary node, propagate from this node.
  replicated_set.insert(cur);
  for (HloInstruction* consumer : users) {
    if (!visited.contains(consumer)) {
      consumer_set.insert(consumer);
      FindReplicateSet(consumer, alias_map, cost_graph, s_val, strategy_map,
                       strategy, output, do_all_gather_after_backward,
                       transpose_inst, replicated_set, boundary_set,
                       consumer_set, visited);
    }
  }

  for (size_t i = 0; i < cur->operand_count(); ++i) {
    HloInstruction* operand = cur->mutable_operand(i);
    operand = PassThroughCustomCallMarkerOperand(operand, cur);

    if (!visited.contains(operand) && !IsAlwaysReplicated(operand) &&
        GetShardingStrategy(operand, strategy_map, cost_graph, s_val)
                .output_sharding == strategy.output_sharding &&
        DimensionsEqual(operand->shape(), cur->shape())) {
      FindReplicateSet(operand, alias_map, cost_graph, s_val, strategy_map,
                       strategy, output, do_all_gather_after_backward,
                       transpose_inst, replicated_set, boundary_set,
                       consumer_set, visited);
    }
  }
}

// Substitute all-reduce strategies with their reduce-scatter variants.
absl::Status GenerateReduceScatter(
    const HloInstructionSequence& sequence, const AliasMap& alias_map,
    const InstructionDepthMap& depth_map, const StrategyMap& strategy_map,
    const CostGraph& cost_graph, absl::Span<const NodeStrategyIdx> s_val,
    const ClusterEnvironment& cluster_env, const AutoShardingOption& option) {
  const std::vector<HloInstruction*>& instructions = sequence.instructions();

  // Propagation ends at output.
  const HloInstruction* output = instructions.back();
  if (IsCustomCallMarker(output)) {
    output = output->operand(0);
  }

  // A debug option: whether to do all-gather after backward pass.
  // This controls the location of all-gather.
  // If true, all-gather happens after backward pass, which is desired for
  // gradient accumulation. If false, all-gather happens before forward pass,
  // which can partitions more tensors.
  bool do_all_gather_after_backward = true;

  // If true, do not actually generate reduce-scatter + all-gather,
  // but generate all-reduce + all-gather instead.
  // This saves less memory but is more friendly to gradient accumulation.
  // This is a temporary workaround due to implementation difficulty.
  // Ideally, we should be able to generate a gradient-accumulation-friendly
  // reduce-scatter + all-gather, but for now it is not easy to implement this
  // in our current system. So we generate a gradient-accumulation-friendly
  // all-reduce + all-gather, which has the same memory consumption but with 50%
  // communication overhead.
  bool use_all_reduce_for_grad_acc = option.reduce_scatter_grad_acc_friendly;

  std::vector<HloInstruction*> insert_all_gather;
  StableHashSet<const HloInstruction*> modified;

  for (HloInstruction* inst : instructions) {
    if (!HasReduceScatterOpportunity(inst, strategy_map, cost_graph, s_val,
                                     modified)) {
      continue;
    }
    const ShardingStrategy& strategy =
        GetShardingStrategy(inst, strategy_map, cost_graph, s_val);
    if (!absl::StrContains(strategy.name, "allreduce")) {
      continue;
    }

    StableHashSet<HloInstruction*> replicated_set;
    StableHashSet<HloInstruction*> boundary_set;
    StableHashSet<HloInstruction*> consumer_set;
    StableHashSet<const HloInstruction*> visited;

    // We allow at most one transpose in the path of replication analysis.
    HloInstruction* transpose_inst = nullptr;

    // Find the replicated set starting from the all-reduce instruction.
    visited.insert(output);
    FindReplicateSet(inst, alias_map, cost_graph, s_val, strategy_map, strategy,
                     output, do_all_gather_after_backward, transpose_inst,
                     replicated_set, boundary_set, consumer_set, visited);

    // Try to reduce the boundary set to its common ancestor
    TryReduceWithCommonAncestor(replicated_set, boundary_set, consumer_set,
                                alias_map);

    // Analyze the instructions after which all-gather should be inserted.
    std::vector<HloInstruction*> need_all_gather;
    for (HloInstruction* node : boundary_set) {
      if (consumer_set.contains(node)) {
        if (AllUsersAreReduce(node)) {
          // If users are reduce, the all-gather cost after this instruction
          // should be small, so we ignore all-gather cost of these
          // instructions.
          replicated_set.insert(node);
        } else {
          need_all_gather.push_back(node);
        }
      }
    }

    // If we do all-gather on some parameters, move this all-gather after
    // backward.
    if (do_all_gather_after_backward && need_all_gather.size() == 1) {
      HloInstruction* point = need_all_gather.front();
      std::vector<HloInstruction*> path;
      HloInstruction* root = point;
      while (true) {
        path.push_back(root);
        if (root->opcode() == HloOpcode::kGetTupleElement) {
          root = PassThroughCustomCallMarkerOperand(root->mutable_operand(0),
                                                    root);
        } else {
          break;
        }
      }

      if (root->opcode() == HloOpcode::kParameter) {
        for (auto x : path) {
          replicated_set.erase(x);
          boundary_set.erase(x);
        }
        need_all_gather.clear();
        for (auto x : replicated_set) {
          auto iter = alias_map.find(x);
          if (iter != alias_map.end() && iter->second == root) {
            boundary_set.insert(x);
            need_all_gather.push_back(x);
            break;
          }
        }
      }
    }

    // Analyze how many parameters can be partitioned if we do this
    // transformation.
    int num_replicated_parameters = 0;
    for (const HloInstruction* node : replicated_set) {
      if (node->opcode() == HloOpcode::kParameter) {
        num_replicated_parameters++;
      }
    }
    for (const HloInstruction* to_split : need_all_gather) {
      if (to_split->users().size() == 1 &&
          to_split->users().front() == output && alias_map.contains(to_split)) {
        // Move the all-gather to its alias parameter.
        num_replicated_parameters++;
      }
    }

    // Print replicated set and boundary set for debugging.
    VLOG(10) << inst->ToString(HloPrintOptions::ShortParsable()) << "\n";
    VLOG(10) << "replicated set (#parameter: " << num_replicated_parameters
             << "):\n";
    for (auto x : replicated_set) {
      VLOG(10) << "  " << x->ToString(HloPrintOptions::ShortParsable()) << "\n";
    }
    VLOG(10) << "boundary set (#incompatible: " << need_all_gather.size()
             << "):\n";
    for (auto x : boundary_set) {
      VLOG(10) << "  " << x->ToString(HloPrintOptions::ShortParsable()) << " "
               << absl::c_linear_search(need_all_gather, x) << "\n";
    }

    // If applicable, replace all-reduce with reduce-scatter by
    // setting instructions' sharding.
    if (num_replicated_parameters >= 1 && need_all_gather.size() <= 1 &&
        replicated_set.size() >= 5) {
      HloSharding output_spec =
          GetReduceScatterOutput(inst, strategy, cluster_env);
      if (IsUndefined(output_spec)) {
        continue;
      }

      VLOG(10) << "SET: " << output_spec.ToString();

      if (absl::StartsWith(strategy.name, "RR = RS x SR")) {
        // If set the sharding for this dot instruction, the SPMD
        // partitioner will generate bad fallback code.
        replicated_set.erase(inst);
      }

      if (use_all_reduce_for_grad_acc) {
        UseAllReduceForGradAcc(replicated_set, inst);
      }

      for (HloInstruction* to_split : replicated_set) {
        SetSharding(to_split, output_spec, inst, transpose_inst, modified);
      }

      if (!option.reduce_scatter_aggressive_partition) {
        // The normal case
        for (HloInstruction* to_split : need_all_gather) {
          SetSharding(to_split, output_spec, inst, transpose_inst, modified);

          if (!do_all_gather_after_backward && to_split->users().size() == 1 &&
              to_split->users().front() == output &&
              alias_map.contains(to_split)) {
            // Move the all-gather to its alias parameter.
            // This partitions more tensors but introduces communication
            // in the forward pass, which is not desired in gradient
            // accumulation.
            SetSharding(alias_map.at(to_split), output_spec, inst,
                        transpose_inst, modified);
            insert_all_gather.push_back(alias_map.at(to_split));
          } else {
            insert_all_gather.push_back(to_split);

            if (to_split->opcode() == HloOpcode::kGetTupleElement &&
                IsCustomCallMarker(to_split->operand(0)) &&
                to_split->users().size() == 1 &&
                to_split->users().front() == output) {
              insert_all_gather.push_back(PassThroughCustomCallMarkerOperand(
                  to_split->mutable_operand(0), to_split));
            }
          }
        }
      } else {
        // Aggressively partition more parameter tensors.
        // This can result in a strategy similar to ZeRO stage 3.
        // NOTE: The combination of this branch with pipeline parallel is not
        // tested.
        for (HloInstruction* to_split : need_all_gather) {
          SetSharding(to_split, output_spec, inst, transpose_inst, modified);

          if (to_split->users().size() == 1 &&
              to_split->users().front() == output &&
              alias_map.contains(to_split)) {
            // Move the all-gather to its alias parameter.
            HloInstruction* param = alias_map.at(to_split);

            // Find the branching point (i.e., skip elementwise ops like
            // convert)
            HloInstruction* cur = param;
            while (cur->users().size() == 1) {
              // TODO(zhuohan): handle tuple.
              CHECK(cur->shape().IsArray());
              SetSharding(cur, output_spec, inst, transpose_inst, modified);
              cur = cur->users().front();
            }
            SetSharding(cur, output_spec, inst, transpose_inst, modified);

            CHECK(!cur->users().empty());

            // Find the first user.
            HloInstruction* first_user = nullptr;
            int64_t min_depth = ((int64_t)1) << 50;
            for (const auto& x : cur->users()) {
              auto iter = depth_map.find(x);
              if (iter == depth_map.end()) {
                LOG(FATAL) << "ERROR: " << x->ToString();
              }
              if (x->opcode() != HloOpcode::kConvolution &&
                  x->opcode() != HloOpcode::kDot) {
                // Only apply this aggressive optimization for dot and conv.
                continue;
              }
              if (iter->second < min_depth) {
                first_user = x;
                min_depth = iter->second;
              }
            }

            if (first_user != nullptr) {
              // Insert an identity to prevent CSE of all-gather.
              HloInstruction* identity = inst->parent()->AddInstruction(
                  HloInstruction::CreateCustomCall(cur->shape(), {cur},
                                                   kIdentityMarker));
              SetSharding(identity, output_spec, inst, transpose_inst,
                          modified);
              ReplaceOperand(first_user, cur, identity);
            }
          }
        }
      }
    }

    VLOG(10) << "-----------------------done\n";
  }

  // Insert all-gather on the output of boundary nodes by setting
  // their shardings. This also works as CSE of all-gather.
  for (HloInstruction* inst : insert_all_gather) {
    HloInstruction* replace_with = inst->parent()->AddInstruction(
        HloInstruction::CreateReshape(inst->shape(), inst));
    replace_with->set_sharding(
        GetShardingStrategy(inst, strategy_map, cost_graph, s_val)
            .output_sharding);
    TF_RETURN_IF_ERROR(inst->ReplaceAllUsesWith(replace_with));
  }
  return absl::OkStatus();
}

void AnnotateShardingWithSimpleHeuristic(
    HloModule* module, const std::string& heuristic, const AliasMap& alias_map,
    const ClusterEnvironment& cluster_env) {
  const Array<int64_t>& device_mesh = cluster_env.device_mesh_;
  const Array<int64_t>& device_mesh_1d = cluster_env.device_mesh_1d_;
  int64_t num_devices = device_mesh.num_elements();

  // Count the non-one mesh dimension.
  size_t mesh_nn_dims = 0;
  for (int dim : device_mesh.dimensions()) {
    if (dim > 1) {
      mesh_nn_dims++;
    }
  }

  // Shard instructions
  HloComputation* entry_computation = module->entry_computation();
  for (HloInstruction* inst : entry_computation->instructions()) {
    if (inst->opcode() == HloOpcode::kParameter) {
      HloSharding output_spec = HloSharding::Replicate();
      inst->set_sharding(output_spec);

      if (heuristic == "shard-largest") {
        std::vector<int64_t> lengths;
        for (int64_t i = 0; i < inst->shape().rank(); ++i) {
          lengths.push_back(inst->shape().dimensions(i));
        }

        std::vector<int> indices = Argsort(lengths);
        int common_dims = std::min(mesh_nn_dims, indices.size());

        if (common_dims < 1) {
          continue;
        }

        if (common_dims == 1) {
          int dim = indices[0];
          int length = lengths[dim];
          if (length % num_devices == 0) {
            output_spec = Tile(inst->shape(), {dim}, {0}, device_mesh_1d);
          }
        } else {
          int dim1 = indices[0];
          int length1 = lengths[dim1];
          int dim0 = indices[1];
          int length0 = lengths[dim0];

          if (length0 % device_mesh.dim(0) == 0 &&
              length1 % device_mesh.dim(1) == 0) {
            output_spec =
                Tile(inst->shape(), {dim0, dim1}, {0, 1}, device_mesh);
          }
        }
      } else if (heuristic == "shard-first") {
        if (inst->shape().rank() > 0 &&
            inst->shape().dimensions(0) % num_devices == 0) {
          output_spec = Tile(inst->shape(), {0}, {0}, device_mesh_1d);
        }
      } else if (heuristic == "shard-last") {
        int64_t last_dim = inst->shape().rank() - 1;
        if (inst->shape().rank() > 0 &&
            inst->shape().dimensions(last_dim) % num_devices == 0) {
          output_spec = Tile(inst->shape(), {last_dim}, {0}, device_mesh_1d);
        }
      } else {
        LOG(FATAL) << "Invalid heuristic: " << heuristic;
      }

      inst->set_sharding(output_spec);
    } else if (inst->opcode() == HloOpcode::kDot) {
      const HloInstruction* lhs = inst->operand(0);
      const HloInstruction* rhs = inst->operand(1);
      const DotDimensionNumbers& dot_dnums = inst->dot_dimension_numbers();
      // const auto& lhs_con_dims = dot_dnums.lhs_contracting_dimensions();
      // const auto& rhs_con_dims = dot_dnums.rhs_contracting_dimensions();
      tsl::protobuf::RepeatedField<int64_t> lhs_space_dims, rhs_space_dims;
      std::tie(lhs_space_dims, rhs_space_dims) =
          GetSpaceDims(lhs->shape(), rhs->shape(), dot_dnums);
    }
  }

  // Meet the alias requirement for the output tuple.
  HloInstruction* output = entry_computation->root_instruction();
  const Shape& out_shape = output->shape();
  ShapeTree<HloSharding> tuple_sharding(out_shape, HloSharding::Replicate());
  std::vector<HloSharding> flattened_shardings;

  std::function<void(HloInstruction*)> get_flattened_shardings;
  get_flattened_shardings = [&](HloInstruction* cur) {
    for (int64_t i = 0; i < cur->operand_count(); ++i) {
      HloInstruction* operand = cur->mutable_operand(i);

      if (operand->shape().IsTuple()) {
        get_flattened_shardings(operand);
      } else {
        if (alias_map.contains(operand)) {
          operand = alias_map.at(operand);
        }
        if (!operand->has_sharding()) {
          operand->set_sharding(HloSharding::Replicate());
        }
        CHECK(operand->has_sharding());
        flattened_shardings.push_back(operand->sharding());
      }
    }
  };
  get_flattened_shardings(output);
  int i = 0;
  for (auto& leaf : tuple_sharding.leaves()) {
    leaf.second = flattened_shardings[i++];
  }
  CHECK_EQ(i, flattened_shardings.size());
  output->set_sharding(HloSharding::Tuple(tuple_sharding));
}

// Filter strategies according to the option.force_batch_dim_to_mesh_dim.
// This can be used to forcibly generate data-parallel strategies.
absl::Status FilterStrategy(const HloInstruction* ins, const Shape& shape,
                            std::unique_ptr<StrategyGroup>& strategy_group,
                            const ClusterEnvironment& cluster_env,
                            const InstructionBatchDimMap& batch_map,
                            const AutoShardingOption& option) {
  int mesh_dim = option.force_batch_dim_to_mesh_dim;
  int batch_dim = batch_map.at(GetBatchDimMapKey(ins));
  const Array<int64_t>& device_mesh = cluster_env.device_mesh_;

  if (shape.dimensions(batch_dim) % device_mesh.dim(mesh_dim) != 0) {
    return absl::InvalidArgumentError(
        "The length of batch dimension is "
        "not divisible by the number of devices");
  }

  std::vector<ShardingStrategy> new_strategies;
  for (auto& stra : strategy_group->strategies) {
    std::vector<int64_t> tensor_dim_to_mesh_dim =
        cluster_env.GetTensorDimToMeshDimWrapper(shape, stra.output_sharding);

    if (device_mesh.dim(mesh_dim) > 1) {
      // If the mesh dim is not one, the output tensor must be
      // tiled along the mesh dim.
      if (tensor_dim_to_mesh_dim[batch_dim] == mesh_dim) {
        new_strategies.push_back(std::move(stra));
      }
    } else {
      // If the mesh dim is one, the output tensor must be replicated
      // on the mesh dim.
      if (tensor_dim_to_mesh_dim[batch_dim] == -1) {
        new_strategies.push_back(std::move(stra));
      }
    }
  }
  CHECK(!new_strategies.empty())
      << ins->ToString() << " does not have any valid strategies";
  strategy_group->strategies = std::move(new_strategies);

  return absl::OkStatus();
}

// Return the output sharding of the reduce-scatter variant of a given strategy.
HloSharding GetReduceScatterOutput(const HloInstruction* ins,
                                   const ShardingStrategy& strategy,
                                   const ClusterEnvironment& cluster_env) {
  const Array<int64_t>& device_mesh = cluster_env.device_mesh_;
  const Array<int64_t>& device_mesh_1d = cluster_env.device_mesh_1d_;

  if (ins->opcode() == HloOpcode::kDot) {
    const DotDimensionNumbers& dot_dnums = ins->dot_dimension_numbers();
    int64_t space_base_dim = dot_dnums.lhs_batch_dimensions_size();

    if (absl::StartsWith(strategy.name, "SR = SS x SR") ||
        absl::StartsWith(strategy.name, "RS = RS x SS")) {
      int mesh_dim0, mesh_dim1;
      std::tie(mesh_dim0, mesh_dim1) = ParseMeshDims(strategy.name);

      if (!IsDivisible(ins, device_mesh, {space_base_dim, space_base_dim + 1},
                       {mesh_dim0, mesh_dim1})) {
        // XLA supports uneven partitioning by adding padding.
        // However, the ShardingSpec in Jax does not support uneven
        // partitioning.
        return Undefined();
      }

      return Tile(ins->shape(), {space_base_dim, space_base_dim + 1},
                  {mesh_dim0, mesh_dim1}, device_mesh);
    }
    if (absl::StartsWith(strategy.name, "SbR = SbSk x SbSk")) {
      int mesh_dim0, mesh_dim1;
      std::tie(mesh_dim0, mesh_dim1) = ParseMeshDims(strategy.name);

      if (!IsDivisible(ins, device_mesh, {0, space_base_dim},
                       {mesh_dim0, mesh_dim1})) {
        // XLA supports uneven partitioning by adding padding.
        // However, the ShardingSpec in Jax does not support uneven
        // partitioning.
        return Undefined();
      }

      return Tile(ins->shape(), {0, space_base_dim}, {mesh_dim0, mesh_dim1},
                  device_mesh);
    }
    if (absl::StartsWith(strategy.name, "RR = RS x SR")) {
      int mesh_dim = absl::StrContains(strategy.name, "{0}") ? 0 : 1;

      if (!IsDivisible(ins, device_mesh, {space_base_dim}, {mesh_dim})) {
        return Undefined();
      }

      return Tile(ins->shape(), {space_base_dim}, {mesh_dim}, device_mesh);
    }
    if (absl::StartsWith(strategy.name, "R = Sk x Sk")) {
      int mesh_dim = 0;

      if (!IsDivisible(ins, device_mesh_1d, {space_base_dim}, {mesh_dim})) {
        return Undefined();
      }

      return Tile(ins->shape(), {space_base_dim}, {mesh_dim}, device_mesh_1d);
    }
  } else if (ins->opcode() == HloOpcode::kConvolution) {
    const ConvolutionDimensionNumbers& conv_dnums =
        ins->convolution_dimension_numbers();
    int out_batch_dim = conv_dnums.output_batch_dimension();
    int out_out_channel_dim = conv_dnums.output_feature_dimension();

    if (absl::StartsWith(strategy.name, "SR = SS x SR") ||
        absl::StartsWith(strategy.name, "RS = RS x SS")) {
      int mesh_dim0, mesh_dim1;
      std::tie(mesh_dim0, mesh_dim1) = ParseMeshDims(strategy.name);

      if (!IsDivisible(ins, device_mesh, {out_batch_dim, out_out_channel_dim},
                       {mesh_dim0, mesh_dim1})) {
        return Undefined();
      }

      return Tile(ins->shape(), {out_batch_dim, out_out_channel_dim},
                  {mesh_dim0, mesh_dim1}, device_mesh);
    }
    if (absl::StartsWith(strategy.name, "R = Sk x Sk")) {
      int mesh_dim = 0;

      if (!IsDivisible(ins, device_mesh_1d, {out_batch_dim}, {mesh_dim})) {
        return Undefined();
      }

      return Tile(ins->shape(), {out_batch_dim}, {mesh_dim}, device_mesh_1d);
    }
  } else if (ins->opcode() == HloOpcode::kReduce) {
    // TODO(zhuohan): support more cases.
    CHECK_EQ(ins->shape().rank(), 1);

    int mesh_dim;
    if (absl::StrContains(strategy.name, "allreduce @ [0]")) {
      mesh_dim = 0;
    } else {
      mesh_dim = 1;
    }

    if (strategy.output_sharding.IsReplicated()) {
      if (absl::StrContains(strategy.name, "1d")) {
        if (!IsDivisible(ins, device_mesh_1d, {0}, {mesh_dim})) {
          return Undefined();
        }

        return Tile(ins->shape(), {0}, {mesh_dim}, device_mesh_1d);
      }
      if (!IsDivisible(ins, device_mesh, {0}, {mesh_dim})) {
        return Undefined();
      }

      return Tile(ins->shape(), {0}, {mesh_dim}, device_mesh);
    }
    if (!IsDivisible(ins, device_mesh_1d, {0}, {0})) {
      return Undefined();
    }

    auto tile_assignment = strategy.output_sharding.tile_assignment().Reshape(
        {cluster_env.total_devices_});
    return HloSharding::Tile(std::move(tile_assignment));

  } else {
    LOG(FATAL) << "Invalid instruction: " << ins->ToString();
  }

  return Undefined();
}

// Return whether an instruction has the opportunity to generate reduce-scatter.
bool HasReduceScatterOpportunity(
    const HloInstruction* inst, const StrategyMap& strategy_map,
    const CostGraph& cost_graph, absl::Span<const NodeStrategyIdx> s_val,
    const StableHashSet<const HloInstruction*>& modified) {
  // If the operand is already modified by other ops, skip this instruction to
  // avoid conflicts.
  for (const HloInstruction* operand : inst->operands()) {
    if (modified.contains(operand)) {
      return false;
    }
  }
  if (modified.contains(inst)) {
    return false;
  }

  if (inst->opcode() == HloOpcode::kReduce && inst->shape().rank() == 1) {
    return true;
  }
  if (inst->opcode() == HloOpcode::kDot) {
    if (GetShardingStrategy(inst->operand(0), strategy_map, cost_graph, s_val)
            .output_sharding.IsReplicated() &&
        GetShardingStrategy(inst->operand(1), strategy_map, cost_graph, s_val)
            .output_sharding.IsReplicated()) {
      // This dot is replicated on all devices. Do not split it.
      // TODO(zhuohan): improve this condition.
      return false;
    }

    return true;
  }
  if (inst->opcode() == HloOpcode::kConvolution) {
    return true;
  }

  return false;
}

}  // namespace spmd

bool IsInstructionBeforeSPMDFullToShardShapeCustomCall(
    const HloInstruction* ins) {
  if (ins->users().empty()) {
    return false;
  } else if (ins->users().size() == 1) {
    return spmd::IsSPMDFullToShardShapeCustomCall(ins->users()[0]);
  } else {
    for (const HloInstruction* user : ins->users()) {
      if (spmd::IsSPMDFullToShardShapeCustomCall(user)) {
        return true;
      }
    }
    return false;
  }
}

std::pair<absl::flat_hash_map<std::string, std::vector<HloSharding>>, bool>
AutoShardingImplementation::SaveAndRemoveShardingAnnotation(
    HloModule* module,
    const absl::flat_hash_set<const HloInstruction*>& instructions_to_shard,
    const absl::flat_hash_set<std::string>& replicated_small_tensors,
    const absl::flat_hash_set<absl::string_view>& execution_threads) {
  absl::flat_hash_map<std::string, std::vector<HloSharding>> preserve_shardings;
  absl::flat_hash_set<HloInstruction*> keep_inst;

  for (const HloComputation* computation :
       module->computations(execution_threads)) {
    for (const auto inst : computation->instructions()) {
      if (inst->opcode() == HloOpcode::kOutfeed ||
          inst->opcode() == HloOpcode::kRecv ||
          inst->opcode() == HloOpcode::kRecvDone ||
          inst->opcode() == HloOpcode::kSend ||
          inst->opcode() == HloOpcode::kSendDone) {
        spmd::SaveShardingForInstruction(inst,
                                         /* save_for_copy_users */ false,
                                         preserve_shardings);
        continue;
      }
      if (IsInstructionBeforeSPMDFullToShardShapeCustomCall(inst) ||
          spmd::IsSPMDShardToFullShapeCustomCall(inst)) {
        spmd::SaveShardingForInstruction(inst,
                                         /* save_for_copy_users */ false,
                                         preserve_shardings);
      }
      if (inst->has_sharding() &&
          spmd::IsShardingMisaligned(inst->sharding(), inst->shape()) &&
          !instructions_to_shard.contains(inst)) {
        LOG(WARNING)
            << "Instruction " << inst->name()
            << " has a user sharding annotation that is misaligned. Shape: "
            << inst->shape().ToString()
            << ". Sharding:" << inst->sharding().ToString();
      }
    }
  }

  if (option_.preserve_shardings ==
      AutoShardingOption::PreserveShardingsType::kKeepAllShardings) {
    // Saves shardings for all instructions.
    for (const HloComputation* computation :
         module->computations(execution_threads)) {
      for (const auto inst : computation->instructions()) {
        spmd::SaveShardingForInstruction(inst,
                                         /* save_for_copy_users */ true,
                                         preserve_shardings);
      }
    }
    return std::make_pair(preserve_shardings, /* module_is_changed */ false);
  }

  bool module_is_changed = false;
  for (HloComputation* computation : module->computations(execution_threads)) {
    bool is_entry_computation = computation->IsEntryComputation();

    for (HloInstruction* ins : computation->instructions()) {
      // Do not remove sharding annotations from instructions replicated as
      // they are small tensors
      if (replicated_small_tensors.count(ins->name())) {
        keep_inst.insert(ins);
        spmd::SaveShardingForInstruction(ins,
                                         /* save_for_copy_users */ false,
                                         preserve_shardings);
        continue;
      }
      // Do not remove entry computation's parameter and root instruction's
      // sharding if preserve_shardings is kKeepInputOutputShardings.
      if (option_.preserve_shardings ==
              AutoShardingOption::PreserveShardingsType::
                  kKeepInputOutputShardings &&
          is_entry_computation &&
          (ins->opcode() == HloOpcode::kParameter || ins->IsRoot())) {
        keep_inst.insert(ins);
        spmd::SaveShardingForInstruction(
            ins,
            /* save_for_copy_users */ ins->opcode() == HloOpcode::kParameter,
            preserve_shardings);
        continue;
      }

      if (ins->opcode() == HloOpcode::kCopy &&
          keep_inst.find(ins->operand(0)) != keep_inst.end()) {
        continue;
      }

      if (ins->opcode() == HloOpcode::kOutfeed ||
          ins->opcode() == HloOpcode::kSend ||
          ins->opcode() == HloOpcode::kSendDone ||
          IsInstructionBeforeSPMDFullToShardShapeCustomCall(ins) ||
          spmd::IsSPMDShardToFullShapeCustomCall(ins) ||
          !instructions_to_shard.contains(ins)) {
        continue;
      }

      if (ins->has_sharding()) {
        module_is_changed |= true;
        ins->clear_sharding();
      }
    }
  }
  return std::make_pair(preserve_shardings, module_is_changed);
}

absl::Status AutoShardingImplementation::CanonicalizeLayouts(
    HloModule* module) {
  if (!module->layout_canonicalization_callback()) {
    LOG(INFO) << "There is no registered layout_canonicalization_callback.";
    return absl::OkStatus();
  }
  TF_ASSIGN_OR_RETURN(auto layouts,
                      module->layout_canonicalization_callback()(*module));
  std::vector<Shape>& argument_shapes = layouts.first;
  Shape& result_shape = layouts.second;
  ComputationLayout entry_computation_layout =
      module->config().entry_computation_layout();
  TF_RETURN_IF_ERROR(
      entry_computation_layout.mutable_result_layout()->CopyLayoutFromShape(
          result_shape));
  CHECK_NE(entry_computation_layout.parameter_count(), 0);
  CHECK_EQ(argument_shapes.size(), entry_computation_layout.parameter_count());
  for (int32_t i = 0; i < entry_computation_layout.parameter_count(); i++) {
    TF_RETURN_IF_ERROR(entry_computation_layout.mutable_parameter_layout(i)
                           ->CopyLayoutFromShape(argument_shapes.at(i)));
  }
  *module->mutable_config().mutable_entry_computation_layout() =
      entry_computation_layout;
  return absl::OkStatus();
}

// Computes the set of instructions that lie outside any manually partitioned
// sub-graphs.
absl::flat_hash_set<const HloInstruction*> ComputeInstructionsToShard(
    const HloModule& module, const HloInstructionSequence& sequence) {
  std::queue<const HloInstruction*> queue;

  for (HloInstruction* instruction : sequence.instructions()) {
    if (spmd::IsSPMDFullToShardShapeCustomCall(instruction)) {
      for (const HloInstruction* user : instruction->users()) {
        if (spmd::IsSPMDShardToFullShapeCustomCall(user)) {
          continue;
        }
        queue.push(user);
      }
    }
  }

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
        if (spmd::IsSPMDShardToFullShapeCustomCall(parameter) ||
            spmd::IsSPMDFullToShardShapeCustomCall(parameter) ||
            parameter == instruction || visited.contains(parameter)) {
          continue;
        }
        queue.push(parameter);
      }
    }

    for (const HloInstruction* user : instruction->users()) {
      if (spmd::IsSPMDShardToFullShapeCustomCall(user) ||
          spmd::IsSPMDFullToShardShapeCustomCall(user) ||
          visited.contains(user)) {
        continue;
      }
      queue.push(user);
    }
    for (const HloInstruction* operand : instruction->operands()) {
      if (spmd::IsSPMDShardToFullShapeCustomCall(operand) ||
          spmd::IsSPMDFullToShardShapeCustomCall(operand) ||
          operand == instruction || visited.contains(operand)) {
        continue;
      }
      queue.push(operand);
    }
  }

  absl::flat_hash_set<const HloInstruction*> to_shard;
  for (HloInstruction* instruction : sequence.instructions()) {
    if (!visited.contains(instruction) &&
        !spmd::IsSPMDFullToShardShapeCustomCall(instruction)) {
      if (HloCollectiveInstruction::ClassOf(instruction)) {
        LOG(FATAL) << "The module contains collective ops not contained within "
                      "a graph surrounded by SPMDFullToShardShape and "
                      "SPMDShardToFullShape custom calls. This case is not yet "
                      "supported.";
      }
      to_shard.insert(instruction);
    }
  }
  return to_shard;
}

AutoShardingImplementation::AutoShardingImplementation(
    const AutoShardingOption& option)
    : option_(option) {}

std::pair<int64_t, int64_t> ReduceMemoryTerms(
    int64_t num_primitives,
    const std::vector<std::pair<spmd::LivenessIdx, spmd::LivenessIdx>>&
        intervals,
    std::vector<std::pair<spmd::LivenessIdx, spmd::LivenessIdx>>&
        reduced_intervals,
    std::vector<absl::btree_set<int64_t>>& reduced_groups) {
  int64_t num_lives = 0;
  for (const auto& interval : intervals) {
    if (interval.first > interval.second) continue;  // Interval undefined
    num_lives = std::max(num_lives, interval.second + 1);
  }
  auto Intervals =
      [intervals](int64_t prim_idx) -> std::pair<int64_t, int64_t> {
    return intervals.at(prim_idx);
  };
  spmd::MemoryTermReducer reducer;
  auto num_terms =
      reducer.Reduce(num_lives, num_primitives, std::move(Intervals));
  reduced_intervals = reducer.GetReducedIntervals();
  reduced_groups = reducer.GetReducedGroups();
  return num_terms;
}

absl::StatusOr<AutoShardingResult> AutoShardingImplementation::RunAutoSharding(
    HloModule* module,
    const absl::flat_hash_set<std::string>& replicated_small_tensors,
    const absl::flat_hash_set<absl::string_view>& execution_threads,
    const absl::flat_hash_map<std::string, const HloInstruction*>&
        sharding_propagation_solution) {
  if (!option_.enable) {
    return AutoShardingResult::kModuleUnchanged;
  }
  bool module_is_changed = false;

  bool set_to_memory_lower_bound = (option_.memory_budget_per_device == 0);

  // Remove CustomCalls with custom_call_target="Sharding" and move their
  // shardings to their input ops.
  absl::flat_hash_map<const HloInstruction*, std::vector<int64_t>>
      unspecified_dims;
  absl::StatusOr<bool> changed = ProcessShardingInstruction(
      module, execution_threads, /*replace_sharding_with_copy=*/true,
      &unspecified_dims, /*saved_root_shardings=*/nullptr,
      /*saved_parameter_shardings=*/nullptr);
  if (!changed.ok()) {
    return changed.status();
  }
  if (changed.value()) {
    module_is_changed = true;
    VLOG(3) << "CustomCalls with custom_call_target=Sharding are removed and "
               "their shardings are moved to their input ops.";
  } else {
    VLOG(3) << "This workload does not have CustomCalls with "
               "custom_call_target=Sharding.";
  }

  // ----- Get a sequential schedule and do liveness analysis -----
  auto size_fn = [](const BufferValue& buffer) {
    return spmd::ByteSizeOfShape(buffer.shape());
  };
  TF_ASSIGN_OR_RETURN(
      HloSchedule schedule,
      ScheduleModule(module, size_fn,
                     ComputationSchedulerToModuleScheduler(DFSMemoryScheduler),
                     execution_threads));
  const HloComputation* entry_computation = module->entry_computation();
  std::unique_ptr<HloAliasAnalysis> alias_analysis =
      HloAliasAnalysis::Run(module).value();

  // Handle donated args by resolving them into input-output aliases. While we
  // want to perform this resolution, we do not want to modify the module, which
  // is why we run the OptimizeInputOutputBufferAlias pass on a clone.
  auto module_clone = module->Clone("");
  OptimizeInputOutputBufferAlias input_output_buffer_alias_optimizer(
      /* registered_buffer_donor_only */ true);
  CHECK_OK(input_output_buffer_alias_optimizer.Run(module_clone.get()));
  const HloInputOutputAliasConfig& input_output_alias_config =
      module_clone->input_output_alias_config();

  spmd::AliasMap alias_map =
      spmd::BuildAliasMap(module, input_output_alias_config);

  TF_ASSIGN_OR_RETURN(
      std::unique_ptr<HloLiveRange> hlo_live_range,
      HloLiveRange::Run(schedule, *alias_analysis, entry_computation));
  absl::flat_hash_map<const HloValue*, HloLiveRange::TimeBound>&
      buffer_live_ranges = hlo_live_range->buffer_live_ranges();
  spmd::LivenessSet liveness_set(hlo_live_range->schedule_end_time() + 1);
  for (const auto& [hlo_value, live_range] : buffer_live_ranges) {
    for (spmd::LivenessIdx i = live_range.start; i <= live_range.end; ++i) {
      liveness_set[i].push_back(hlo_value);
    }
  }
  VLOG(10) << hlo_live_range->ToString();
  XLA_VLOG_LINES(10, spmd::PrintLivenessSet(liveness_set));
  const HloInstructionSequence& sequence =
      hlo_live_range->flattened_instruction_sequence();

  const absl::flat_hash_set<const HloInstruction*>& instructions_to_shard =
      ComputeInstructionsToShard(*module, sequence);

  std::pair<absl::flat_hash_map<std::string, std::vector<HloSharding>>, bool>
      preserve_shardings_result = SaveAndRemoveShardingAnnotation(
          module, instructions_to_shard, replicated_small_tensors,
          execution_threads);
  absl::flat_hash_map<std::string, std::vector<HloSharding>>
      preserve_shardings = std::move(preserve_shardings_result.first);
  module_is_changed |= preserve_shardings_result.second;

  absl::flat_hash_map<const HloInstruction*, int64_t>
      instruction_execution_counts = spmd::ComputeInstructionExecutionCounts(
          module, option_.loop_iteration_count_estimate);

  // ----- Analyze the batch dim -----
  spmd::InstructionBatchDimMap batch_dim_map;
  // TODO(yuemmawang) Enable the batch_dim_map if it becomes helpful. This is
  // supposed to make the solver faster, but it makes it much much slower for
  // both 1D and 2D mesh shapes.
  // batch_dim_map = spmd::BuildInstructionBatchDimMap(sequence);

  // ----- Read parameters of device mesh -----
  Array<int64_t> original_device_mesh(option_.device_mesh_shape);
  original_device_mesh.SetValues(option_.device_mesh_ids);
  const int64_t original_memory_budget = option_.memory_budget_per_device;

  std::vector<std::vector<int64_t>> partial_mesh_shapes;
  if (option_.solve_nd_sharding_iteratively) {
    // Generate partial mesh shapes to optimize iteratively.
    partial_mesh_shapes = spmd::DecomposeMeshShapes(option_.device_mesh_shape);
  } else {
    partial_mesh_shapes = {option_.device_mesh_shape};
  }

  std::unique_ptr<CallGraph> call_graph = CallGraph::Build(module);

  HloCostAnalysis::Options hlo_cost_analysis_options{
      .shape_size = [](const Shape& shape) {
        return spmd::ByteSizeOfShape(shape);
      }};
  HloCostAnalysis hlo_cost_analysis(hlo_cost_analysis_options);
  CHECK_OK(module->entry_computation()->Accept(&hlo_cost_analysis));
  for (size_t mesh_idx = 0; mesh_idx < partial_mesh_shapes.size(); ++mesh_idx) {
    // Adjust existing shardings with current partial mesh shapes.
    std::vector<int64_t> mesh_shape = partial_mesh_shapes[mesh_idx];
    LOG(INFO) << "Processing partial mesh shape: "
              << spmd::ToString(mesh_shape);
    Array<int64_t> device_mesh(mesh_shape);

    int64_t total_devices = 1;
    for (int64_t i : mesh_shape) {
      total_devices *= i;
    }
    if (mesh_idx != partial_mesh_shapes.size() - 1) {
      absl::StatusOr<bool> changed = spmd::AdjustShardingsWithPartialMeshShape(
          sequence.instructions(), instructions_to_shard, mesh_shape,
          total_devices,
          /* crash_on_error */ !option_.try_multiple_mesh_shapes);
      if (changed.ok()) {
        LOG(INFO)
            << "Shardings are adjusted based on current partial mesh shape: "
            << *changed;
      } else {
        return changed.status();
      }
    }
    if (option_.device_mesh_ids.size() == total_devices) {
      // It is unclear what device order to use for partial meshes. So we only
      // use the actual device order only for the final full mesh.
      device_mesh.SetValues(option_.device_mesh_ids);
    } else {
      std::vector<int64_t> device_mesh_ids =
          std::vector<int64_t>(total_devices);
      std::iota(device_mesh_ids.begin(), device_mesh_ids.end(), 0);
      device_mesh.SetValues(device_mesh_ids);
    }

    // TODO (zhuohan): Include the prof result as an option.
    spmd::ProfilingResult prof_result;
    spmd::ClusterEnvironment cluster_env(
        original_device_mesh, device_mesh, option_.device_mesh_alpha,
        option_.device_mesh_beta, prof_result, option_);

    XLA_VLOG_LINES(6, module->ToString());
    const int64_t memory_lower_bound = spmd::MemoryBudgetLowerBound(
        *module, instructions_to_shard, liveness_set, *alias_analysis,
        device_mesh.num_elements(), preserve_shardings);
    const float memory_lower_bound_gb =
        static_cast<float>(memory_lower_bound) / (1024 * 1024 * 1024);
    LOG(INFO) << "Memory consumption lower bound is " << memory_lower_bound_gb
              << " GB.";
    if (set_to_memory_lower_bound) {
      LOG(INFO)
          << "--xla_tpu_auto_spmd_partitioning_memory_budget_gb is 0, and "
             "--xla_tpu_auto_spmd_partitioning_memory_budget_ratio is "
          << option_.memory_budget_ratio
          << ", so setting option.memory_budget_per_device to "
          << memory_lower_bound_gb << " x " << option_.memory_budget_ratio
          << " = " << memory_lower_bound_gb * option_.memory_budget_ratio
          << " GB";
      option_.memory_budget_per_device =
          memory_lower_bound * std::abs(option_.memory_budget_ratio);
      // TODO(b/341299984): Document this flag syntax, or automate the behavior.
      if (option_.memory_budget_ratio < 0) {
        option_.memory_overbudget_coeff = -1.0;  // Disables the soft constraint
      }
    } else if (option_.memory_budget_per_device > 0) {
      option_.memory_budget_per_device = original_memory_budget *
                                         original_device_mesh.num_elements() /
                                         device_mesh.num_elements();
      LOG(INFO) << "Setting option.memory_budget_per_device to "
                << option_.memory_budget_per_device;
    }

    if (!option_.force_simple_heuristic.empty()) {
      AnnotateShardingWithSimpleHeuristic(
          module, option_.force_simple_heuristic, alias_map, cluster_env);
      return AutoShardingResult::kModuleChangedShardingPerformed;
    }

    if (option_.force_batch_dim_to_mesh_dim >= 0) {
      spmd::DisableIncompatibleMixedMeshShapeAndForceBatchDim(
          batch_dim_map, sequence.instructions(), device_mesh.num_elements(),
          option_);
    }

    // ----- Analyze depth -----
    spmd::InstructionDepthMap ins_depth_map;
    ins_depth_map = spmd::BuildInstructionDepthMap(sequence, batch_dim_map);

    // ----- Build strategies and costs -----
    spmd::StrategyMap strategy_map;
    spmd::StrategyGroups strategy_groups;
    spmd::AssociativeDotPairs associative_dot_pairs;
    TF_ASSIGN_OR_RETURN(
        std::tie(strategy_map, strategy_groups, associative_dot_pairs),
        BuildStrategyAndCost(sequence, module, instructions_to_shard,
                             instruction_execution_counts, ins_depth_map,
                             batch_dim_map, alias_map, cluster_env, option_,
                             *call_graph, hlo_cost_analysis,
                             option_.try_multiple_mesh_shapes));
    spmd::AliasSet alias_set =
        spmd::BuildAliasSet(module, input_output_alias_config, strategy_map);
    if (absl::Status alias_set_status = CheckAliasSetCompatibility(
            alias_set, strategy_groups, sequence,
            /* crash_at_error */ !option_.try_multiple_mesh_shapes);
        !alias_set_status.ok()) {
      return alias_set_status;
    }
    XLA_VLOG_LINES(8, PrintStrategyMap(strategy_map, sequence));

    // ----- Build cost graph and merge unimportant nodes -----
    spmd::CostGraph cost_graph(strategy_groups, associative_dot_pairs);
    cost_graph.Simplify(option_.simplify_graph);

    // ----- Build & reduce node and edge intervals -----
    std::vector<absl::flat_hash_set<spmd::EdgeIdx>> node_to_edges(
        strategy_groups.size());
    spmd::EdgeIdx edge_idx = 0;
    for (const auto& [edge, _] : cost_graph.edge_costs_) {
      node_to_edges[edge.second].insert(edge_idx);
      ++edge_idx;
    }
    const absl::flat_hash_map<const HloValue*, HloLiveRange::TimeBound>&
        buffer_live_ranges = hlo_live_range->buffer_live_ranges();
    absl::flat_hash_map<spmd::NodeIdx, HloLiveRange::TimeBound>
        node_to_time_bound;
    absl::flat_hash_map<spmd::EdgeIdx, HloLiveRange::TimeBound>
        edge_to_time_bound;
    for (const auto& [value, time_bound] : buffer_live_ranges) {
      const HloInstruction* instruction = value->instruction();
      const ShapeIndex& index = value->index();
      if (instruction->shape().IsTuple() && index.empty()) continue;
      const spmd::StrategyGroup* strategy_group =
          strategy_map.at(instruction).get();
      const spmd::NodeIdx node_idx =
          strategy_group->GetSubStrategyGroup(index)->node_idx;
      if (node_idx < 0) continue;
      node_to_time_bound[node_idx] = time_bound;
      for (const spmd::EdgeIdx edge_idx : node_to_edges[node_idx]) {
        edge_to_time_bound[edge_idx] = time_bound;
      }
    }
    std::vector<std::pair<spmd::LivenessIdx, spmd::LivenessIdx>> node_intervals,
        edge_intervals;
    for (spmd::NodeIdx node_idx = 0; node_idx < strategy_groups.size();
         ++node_idx) {
      std::pair<spmd::LivenessIdx, spmd::LivenessIdx> interval;
      if (auto time_bound = node_to_time_bound.find(node_idx);
          time_bound != node_to_time_bound.end()) {
        interval.first = time_bound->second.start;
        interval.second = time_bound->second.end;
      } else {
        interval.first = std::numeric_limits<int64_t>::max();
        interval.second = 0;
      }
      node_intervals.push_back(std::move(interval));
    }
    for (spmd::EdgeIdx edge_idx = 0; edge_idx < cost_graph.edge_costs_.size();
         ++edge_idx) {
      std::pair<spmd::LivenessIdx, spmd::LivenessIdx> interval;
      if (auto time_bound = edge_to_time_bound.find(edge_idx);
          time_bound != edge_to_time_bound.end()) {
        interval.first = time_bound->second.start;
        interval.second = time_bound->second.end;
      } else {
        interval.first = std::numeric_limits<int64_t>::max();
        interval.second = 0;
      }
      edge_intervals.push_back(std::move(interval));
    }
    const absl::Time term_reduction_start_time = absl::Now();
    std::vector<std::pair<spmd::LivenessIdx, spmd::LivenessIdx>>
        reduced_node_intervals, reduced_edge_intervals;
    std::vector<absl::btree_set<int64_t>> reduced_node_groups,
        reduced_edge_groups;
    auto num_node_terms =
        ReduceMemoryTerms(strategy_groups.size(), node_intervals,
                          reduced_node_intervals, reduced_node_groups);
    auto num_edge_terms =
        ReduceMemoryTerms(cost_graph.edge_costs_.size(), edge_intervals,
                          reduced_edge_intervals, reduced_edge_groups);
    const absl::Time term_reduction_end_time = absl::Now();
    const auto term_reduction_duration =
        term_reduction_end_time - term_reduction_start_time;
    LOG(INFO) << "Memory Term Reducer took "
              << absl::ToInt64Milliseconds(term_reduction_duration)
              << " ms and reduced the number of terms from "
              << num_node_terms.first + num_edge_terms.first << " to "
              << num_node_terms.second + num_edge_terms.second;

    // ----- Call the ILP Solver -----
    spmd::AutoShardingSolverOutput output;
    std::string request_name = absl::StrCat("mesh_idx_", mesh_idx);
    auto solver_result =
        Solve(*module, *hlo_live_range, strategy_map, strategy_groups,
              cost_graph, alias_set, reduced_node_intervals,
              reduced_edge_intervals, reduced_node_groups, reduced_edge_groups,
              option_, request_name, sharding_propagation_solution);
    if (solver_result.skip_auto_sharding) {
      return AutoShardingResult::kModuleUnchangedNoShardingPerformed;
    } else if (!solver_result.status.ok()) {
      return AutoShardingResult::kModuleUnchanged;
    } else {
      TF_ASSIGN_OR_RETURN(auto solution, solver_result.status);
      output = solution;
      if (mesh_idx == partial_mesh_shapes.size() - 1) {
        this->solver_optimal_objective_value_ = output.cost;
      }
    }

    XLA_VLOG_LINES(5, PrintAutoShardingSolution(
                          sequence, liveness_set, strategy_map, strategy_groups,
                          cost_graph, output.s_val, output.cost));
    XLA_VLOG_LINES(6, PrintSolutionMemoryUsage(liveness_set, strategy_map,
                                               cost_graph, output.s_val));

    // ----- Substitute all-reduce with reduce-scatter -----
    if (option_.prefer_reduce_scatter) {
      TF_RETURN_IF_ERROR(GenerateReduceScatter(
          sequence, alias_map, ins_depth_map, strategy_map, cost_graph,
          output.s_val, cluster_env, option_));
    }
    // ----- Set Sharding -----
    SetHloSharding(sequence, instructions_to_shard, strategy_map, cost_graph,
                   output.s_val, (mesh_idx == partial_mesh_shapes.size() - 1));

    if (mesh_idx == partial_mesh_shapes.size() - 1) {
      if (!option_.post_process) {
        continue;
      }
      if (!SetHloShardingPostProcessing(
               sequence, instructions_to_shard, strategy_map, cost_graph,
               output.s_val, cluster_env,
               /* crash_at_error */ !option_.try_multiple_mesh_shapes,
               preserve_shardings)
               .ok()) {
        return AutoShardingResult::kModuleUnchanged;
      }
    } else {
      spmd::RecoverShardingsFromPartialMesh(sequence, preserve_shardings);
    }
  }

  if (VLOG_IS_ON(1)) {
    spmd::CheckHloSharding(sequence, instructions_to_shard,
                           original_device_mesh.num_elements());
  }
  module_is_changed = true;

  if (VLOG_IS_ON(1)) {
    spmd::CheckUserShardingPreservation(module, preserve_shardings);
  }

  // ----- Canonicalize layouts based on LayoutCanonicalizationCallback. -----
  TF_RETURN_IF_ERROR(CanonicalizeLayouts(module));

  for (HloInstruction* instruction : sequence.instructions()) {
    if (!instructions_to_shard.contains(instruction)) {
      instruction->set_sharding(
          HloSharding::Single(instruction->shape(), HloSharding::Manual()));
    }
  }

  for (HloInstruction* instruction : sequence.instructions()) {
    if (spmd::IsSPMDFullToShardShapeCustomCall(instruction)) {
      CHECK(instruction->has_sharding());
      CHECK(instruction->sharding().IsManual());
      CHECK(instruction->operand(0)->has_sharding());
      CHECK(!instruction->operand(0)->sharding().IsManual());
    } else if (spmd::IsSPMDShardToFullShapeCustomCall(instruction)) {
      CHECK(instruction->has_sharding());
      CHECK(!instruction->sharding().IsManual());
      CHECK(instruction->operand(0)->has_sharding());
      CHECK(instruction->operand(0)->sharding().IsManual());
    }
  }

  return module_is_changed ? AutoShardingResult::kModuleChangedShardingPerformed
                           : AutoShardingResult::kModuleUnchanged;
}

bool ModuleHasUserShardings(const HloModule* module) {
  bool has_shardings = false;
  for (auto computation : module->computations()) {
    for (auto instruction : computation->instructions()) {
      if (instruction->has_sharding()) {
        has_shardings = true;
        break;
      }
    }
    if (has_shardings) {
      break;
    }
  }
  return has_shardings;
}

bool ModuleIsManuallyPartitioned(const HloModule* module) {
  for (const HloComputation* computation : module->computations()) {
    for (const HloInstruction* instruction : computation->instructions()) {
      if (spmd::IsSPMDFullToShardShapeCustomCall(instruction) ||
          spmd::IsSPMDShardToFullShapeCustomCall(instruction)) {
        return true;
      }
    }
  }
  return false;
}

bool IsSmallTensor(const HloInstruction* ins,
                   const AutoShardingOption& option) {
  return spmd::ByteSizeOfShape(ins->shape()) <= option.small_tensor_byte_size;
}

bool ShardedOnTooManyMeshAxes(const HloModule& module) {
  for (const auto* computation : module.computations()) {
    for (const auto* instruction : computation->instructions()) {
      if (instruction->has_sharding() && instruction->sharding().IsTiled() &&
          spmd::NumTileDimensions(instruction->sharding()) >= 3) {
        return true;
      }
    }
  }
  return false;
}

bool HasUnsupportedNestedTuples(const HloModule& module) {
  for (const auto* computation : module.computations()) {
    for (const auto* instruction : computation->instructions()) {
      if (instruction->opcode() == HloOpcode::kConditional) {
        for (const HloInstruction* operand : instruction->operands()) {
          if (ShapeUtil::IsNestedTuple(operand->shape())) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

std::unique_ptr<HloModule> CloneModule(const HloModule* module) {
  auto module_clone = module->Clone("");
  module_clone->set_layout_canonicalization_callback(
      module->layout_canonicalization_callback());
  return module_clone;
}

AutoSharding::AutoSharding(const AutoShardingOption& option)
    : option_(option) {}

absl::StatusOr<bool> AutoSharding::Run(
    HloModule* module,
    const absl::flat_hash_set<absl::string_view>& execution_threads) {
  if (!option_.enable) {
    return false;
  }
  LOG(INFO) << "Starting the auto sharding pass";

  if (ShardedOnTooManyMeshAxes(*module)) {
    LOG(FATAL) << "The input module contains sharding annotations "  // Crash OK
                  "over a mesh with too many axes (>2). This case is currently "
                  "not well supported.";
    return false;
  }

  // TODO(b/332951306): Remove this check once nested tuples are supported
  // everywhere
  if (HasUnsupportedNestedTuples(*module)) {
    LOG(FATAL) << "The input module contains nested tuples "  // Crash OK
                  "which we do not currently support well. See b/332951306 to "
                  "track progress on this.";
    return false;
  }

  XLA_VLOG_LINES(6,
                 absl::StrCat("Before auto sharding:\n", module->ToString()));
  DumpHloModuleIfEnabled(*module, "before_auto_spmd_sharding");

  absl::Time start_time = absl::Now();
#if !defined(__APPLE__)
  // Streamz metrics.
  metrics::RecordAutoShardingInvocations();
#endif

  TF_RETURN_IF_ERROR(module->RemoveUnusedComputations());

  TF_RETURN_IF_ERROR(option_.CheckAndSetup());
  LOG(INFO) << "AutoShardingOptions:\n" << option_.ToString();

  absl::flat_hash_set<std::string> replicated_small_tensors;
  if (option_.small_tensor_byte_size > 0) {
    for (auto computation : module->computations()) {
      for (auto instruction : computation->instructions()) {
        if (!instruction->has_sharding() &&
            IsSmallTensor(instruction, option_)) {
          VLOG(1) << "Replicated small tensor: " << instruction->name();
          instruction->set_sharding(
              instruction->shape().IsTuple()
                  ? HloSharding::SingleTuple(instruction->shape(),
                                             HloSharding::Replicate())
                  : HloSharding::Replicate());
          replicated_small_tensors.insert(std::string(instruction->name()));
        }
      }
    }
  }

  // Run HloConstantSplitter for modules with manually partitioned sub-graphs to
  // avoid having constant ops that are used as part of such manually
  // partitioned sub-graphs, as well as outside those, leading to conflicts
  // during sharding. However, constant splitting can cause increased
  // auto-sharding times, and hence we enable this only when needed.
  bool module_is_manually_partitioned = ModuleIsManuallyPartitioned(module);
  if (module_is_manually_partitioned) {
    HloConstantSplitter constant_splitter(
        /*split_expressions=*/option_.enable_expression_constant_splitter,
        /*extra_constraints=*/spmd::OpEncountersShardToFull);
    CHECK_OK(constant_splitter.Run(module, execution_threads));
    CHECK_OK(HloDCE().Run(module, execution_threads));
  }

  std::vector<std::vector<int64_t>> mesh_shapes;
  if (option_.try_multiple_mesh_shapes || module_is_manually_partitioned) {
    bool asymmetrical_mesh_dims = false;
    for (size_t i = 0; i < option_.device_mesh_shape.size(); ++i) {
      if (option_.device_mesh_beta[0] != option_.device_mesh_beta[i] ||
          option_.device_mesh_alpha[0] != option_.device_mesh_alpha[i]) {
        asymmetrical_mesh_dims = true;
        break;
      }
    }

    mesh_shapes = spmd::InferOrEnumerateMeshShapesToTry(
        *module,
        absl::c_accumulate(option_.device_mesh_shape, 1,
                           [](int64_t a, int64_t b) { return a * b; }),
        option_.device_mesh_shape.size(),
        /* symmetrical_mesh_dims */ !asymmetrical_mesh_dims);
  } else {
    mesh_shapes.push_back(option_.device_mesh_shape);
  }

  CHECK(option_.try_multiple_mesh_shapes || mesh_shapes.size() == 1)
      << "Auto-sharding cannot infer a single appropriate mesh shape for this "
         "HLO, and AutoShardingption::try_multiple_mesh_shapes is set to "
         "false. Please re-run with the option set to true.";

  if (module->entry_computation()->num_parameters() > 0) {
    HloInstruction* parameter_instruction =
        module->entry_computation()->parameter_instruction(0);
    if (parameter_instruction->shape().IsTuple() &&
        parameter_instruction->has_sharding()) {
      CHECK_EQ(module->entry_computation()->num_parameters(), 1);
      parameter_instruction->set_sharding(
          spmd::ReplaceGivenShardingsWithUnknownForTuple(
              parameter_instruction->sharding(), parameter_instruction->shape(),
              module->config()
                  .allow_spmd_sharding_propagation_to_parameters()));
    }
  }

  HloInstruction* root_instruction =
      module->entry_computation()->root_instruction();
  if (root_instruction->shape().IsTuple() && root_instruction->has_sharding()) {
    root_instruction->set_sharding(
        spmd::ReplaceGivenShardingsWithUnknownForTuple(
            root_instruction->sharding(), root_instruction->shape(),
            module->config().allow_spmd_sharding_propagation_to_output()));
  }

  absl::flat_hash_map<std::string, const HloInstruction*>
      sharding_propagation_solution;
  std::unique_ptr<HloModule> module_with_default_solution = nullptr;
  if (option_.use_sharding_propagation_for_default_shardings) {
    module_with_default_solution = CloneModule(module);
    // TODO(pratikf): Ensure that we're passing the correct custom call
    // sharding helper to the sharding propagation pass.
    auto sharding_prop = ShardingPropagation(
        /*is_spmd */ true, /*propagate_metadata */ false,
        /*allow_spmd_sharding_propagation_to_output*/
        module->config().allow_spmd_sharding_propagation_to_output(),
        module->config().allow_spmd_sharding_propagation_to_parameters(),
        /*cse_prevention_only */ false,
        /*sharding_helper*/ nullptr);

    CHECK_OK(sharding_prop.Run(module_with_default_solution.get(),
                               execution_threads));
    VLOG(6) << module_with_default_solution->ToString();
    for (const auto computation :
         module_with_default_solution->computations()) {
      for (const auto instruction : computation->instructions()) {
        if (instruction->has_sharding()) {
          sharding_propagation_solution[instruction->name()] = instruction;
        }
      }
    }
  }

  size_t num_meshes = mesh_shapes.size();
  std::vector<std::unique_ptr<HloModule>> modules(num_meshes);
  std::vector<absl::StatusOr<AutoShardingResult>> changed(
      num_meshes, AutoShardingResult::kModuleUnchanged);
  std::vector<double> objective_values(num_meshes, -1);

  VLOG(1) << "Original mesh shape "
          << spmd::ToString(option_.device_mesh_shape);
  double min_objective_value = std::numeric_limits<double>::max();
  int min_mesh_shape_index = -1;
  bool skip_auto_sharding = true;
  for (size_t i = 0; i < mesh_shapes.size(); ++i) {
    VLOG(1) << "Trying mesh shape " << spmd::ToString(mesh_shapes[i]);
    AutoShardingOption this_option = option_;
    this_option.device_mesh_shape = mesh_shapes[i];
    if (this_option.device_mesh_shape.size() !=
        this_option.device_mesh_alpha.size()) {
      this_option.device_mesh_alpha.clear();
      this_option.device_mesh_beta.clear();
      TF_RETURN_IF_ERROR(this_option.CheckAndSetup());
    }
    auto pass = new AutoShardingImplementation(this_option);
    auto module_clone = CloneModule(module);
    auto pass_result =
        pass->RunAutoSharding(module_clone.get(), replicated_small_tensors,
                              execution_threads, sharding_propagation_solution);

    changed[i] = pass_result;
    objective_values[i] = pass->GetSolverOptimalObjectiveValue();
    modules[i] = std::move(module_clone);
    delete pass;
    if (!pass_result.ok()) {
      VLOG(1) << "Mesh shape " << spmd::ToString(mesh_shapes[i])
              << " led to the following error: "
              << pass_result.status().message();
      continue;
    }
    VLOG(1) << "Mesh shape " << spmd::ToString(mesh_shapes[i])
            << " has objective value " << objective_values[i];
    if (objective_values[i] >= 0 && min_objective_value > objective_values[i]) {
      min_mesh_shape_index = i;
      min_objective_value = objective_values[i];
    }
    if (pass_result.ok() &&
        pass_result.value() !=
            AutoShardingResult::kModuleUnchangedNoShardingPerformed) {
      skip_auto_sharding = false;
    }
  }

  absl::StatusOr<bool> module_is_changed;
  if (skip_auto_sharding) {
    module_is_changed = false;  // The auto-sharding solver timed out.
  } else {
    std::string trying_to_find;
    if (option_.try_multiple_mesh_shapes) {
      trying_to_find = "a device mesh (and the corresponding shardings)";
    } else {
      trying_to_find = "shardings";
    }
    CHECK_GE(min_mesh_shape_index, 0)
        << "The auto-sharding pass could not find " << trying_to_find
        << " that works for this input. This could be the result of a low "
           "memory budget (please refer to the "
           "`--xla_tpu_auto_spmd_partitioning_memory_budget_ratio` flag to set "
           "a higher budget). If you think you have set a reasonably large "
           "memory budget, please report this as a bug.";

    if (!changed[min_mesh_shape_index].ok()) {
      module_is_changed = changed[min_mesh_shape_index].status();
    } else {
      solver_optimal_objective_value_ = min_objective_value;
      if (changed[min_mesh_shape_index].value() ==
          AutoShardingResult::kModuleChangedShardingPerformed) {
        VLOG(1) << "Choosing mesh shape "
                << spmd::ToString(mesh_shapes[min_mesh_shape_index])
                << " which had the minimal solver objective value of "
                << min_objective_value;
        chosen_mesh_shape_ = mesh_shapes[min_mesh_shape_index];
        TF_RETURN_IF_ERROR(
            modules[min_mesh_shape_index]->RemoveUnusedComputations());
        const std::vector<HloComputation*>& original_module_computations =
            module->MakeComputationSorted();
        const std::vector<HloComputation*>& clone_module_computations =
            modules[min_mesh_shape_index]->MakeComputationSorted();
        if (original_module_computations.size() !=
            clone_module_computations.size()) {
          return absl::InternalError(
              "The cloned and the original modules do not have the same number "
              "of computations. This is a bug and should be reported.");
        }

        absl::flat_hash_map<HloComputation*, HloComputation*>
            computation_replacements;
        for (size_t i = 0; i < original_module_computations.size(); ++i) {
          HloComputation* original_computation =
              original_module_computations[i];
          HloComputation* new_computation = clone_module_computations[i];
          computation_replacements[original_computation] = new_computation;
        }

        module->ReplaceComputations(computation_replacements);
        module->MoveComputationsFrom(modules[min_mesh_shape_index].get());

        *module->mutable_config().mutable_entry_computation_layout() =
            modules[min_mesh_shape_index]->entry_computation_layout();
        module->input_output_alias_config() =
            modules[min_mesh_shape_index]->input_output_alias_config();
        module->buffer_donor_config() =
            modules[min_mesh_shape_index]->buffer_donor_config();

        module_is_changed = true;
      } else if (changed[min_mesh_shape_index].value() ==
                 AutoShardingResult::kModuleUnchanged) {
        module_is_changed = false;
      } else {
        module_is_changed = false;
      }
    }
  }

  absl::Time end_time = absl::Now();
  auto duration = end_time - start_time;
  LOG(INFO) << "Auto Sharding took " << absl::ToInt64Seconds(duration)
            << " seconds";
#if !defined(__APPLE__)
  metrics::RecordAutoShardingCompilationTime(
      absl::ToInt64Microseconds(duration));
#endif

  XLA_VLOG_LINES(6, absl::StrCat("After auto sharding:\n", module->ToString()));
  DumpHloModuleIfEnabled(*module, "after_auto_spmd_sharding");

  if (skip_auto_sharding) {
    LOG(FATAL) << "The auto-sharding solver has timed out without a solution.";
  }

  return module_is_changed;
}

absl::StatusOr<bool> DummyAutoSharding::Run(
    HloModule* module,
    const absl::flat_hash_set<absl::string_view>& execution_threads) {
  // ----- Set Dummy Replicated Sharding -----
  HloComputation* entry = module->entry_computation();

  for (HloInstruction* inst : entry->instructions()) {
    const Shape& out_shape = inst->shape();
    if (out_shape.IsTuple()) {
      ShapeTree<HloSharding> tuple_sharding(out_shape,
                                            HloSharding::Replicate());
      inst->set_sharding(HloSharding::Tuple(tuple_sharding));
    } else {
      inst->set_sharding(HloSharding::Replicate());
    }
  }

  return true;
}

}  // namespace xla

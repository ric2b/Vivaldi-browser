/* Copyright 2016 The OpenXLA Authors.

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

#include "xla/service/gpu/transforms/fusion_merger.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/service/gpu/gpu_fusible.h"
#include "xla/service/gpu/model/gpu_hlo_cost_analysis.h"
#include "xla/service/gpu/model/gpu_performance_model.h"
#include "xla/service/gpu/model/gpu_performance_model_base.h"
#include "xla/service/hlo_cost_analysis.h"
#include "xla/service/hlo_graph_dumper.h"
#include "xla/service/instruction_fusion.h"
#include "xla/shape_util.h"
#include "xla/stream_executor/device_description.h"
#include "xla/util.h"
#include "tsl/platform/errors.h"
#include "tsl/platform/status.h"

namespace xla {
namespace gpu {

// For each fusion F, attempts to fuse F into *all* of F's users (does not fuse
// if can't fuse into at least one).
class FusionInstructionMerger {
 public:
  explicit FusionInstructionMerger(
      HloComputation* computation, const se::DeviceDescription& gpu_device_info,
      HloCostAnalysis::ShapeSizeFunction shape_size_function)
      : computation_(computation),
        shape_size_function_(shape_size_function),
        gpu_device_info_(gpu_device_info),
        dump_fusion_visualization_(computation->parent()
                                       ->config()
                                       .debug_options()
                                       .xla_dump_fusion_visualization()) {}

  absl::Status Run();

  bool changed() const { return changed_; }

 private:
  FusionDecision ShouldFuse(HloInstruction* producer);
  absl::Status FuseIntoAllUsers(HloInstruction* producer);

  HloComputation* computation_;
  HloCostAnalysis::ShapeSizeFunction shape_size_function_;
  // Many cheap checks can prevent fusion merging - postpone execution of full
  // HLO cost analysis of the computation so that it may be not needed at all.
  std::optional<GpuHloCostAnalysis> cost_analysis_;
  FusionInfoCache fusion_info_cache_;
  const se::DeviceDescription& gpu_device_info_;
  bool changed_ = false;
  bool dump_fusion_visualization_ = false;

  // Fusion instruction merge stats.
  int total_visited_ = 0;
  int total_merged_ = 0;
  int num_fail_no_users_ = 0;
  int num_fail_not_loop_fusion_ = 0;
  int num_fail_merge_all_users_ = 0;
  int num_fail_inefficient_fusion_emitter_ = 0;
  int num_fail_fusion_too_large_ = 0;
  int num_fail_uncoalesced_read_ = 0;
  int num_fail_slower_if_fused_ = 0;

  FusionInstructionMerger(const FusionInstructionMerger&) = delete;
  FusionInstructionMerger& operator=(const FusionInstructionMerger&) = delete;
};

absl::Status FusionInstructionMerger::FuseIntoAllUsers(
    HloInstruction* producer) {
  // Merge fused instructions from 'fusion' into each user.
  std::vector<HloInstruction*> users = producer->users();
  for (HloInstruction* user : users) {
    if (dump_fusion_visualization_) {
      RegisterFusionState(
          *computation_,
          absl::StrCat("About to fuse |", producer->name(), "| into |",
                       user->name(), "| inside FusionMerger"),
          /*consumer=*/*user,
          /*producer=*/producer);
    }

    TF_RETURN_IF_ERROR(cost_analysis_->RemoveInstruction(user));

    // Wrap consumers which are not fusions first.
    HloInstruction* consumer = user;
    if (consumer->opcode() != HloOpcode::kFusion) {
      consumer = computation_->AddInstruction(HloInstruction::CreateFusion(
          user->shape(), ChooseFusionKind(*producer, *user), user));
      TF_CHECK_OK(computation_->ReplaceInstruction(user, consumer));
    }

    consumer->MergeFusionInstruction(producer);
    TF_RETURN_IF_ERROR(cost_analysis_->RevisitInstruction(consumer));
    fusion_info_cache_.Invalidate(consumer);

    if (dump_fusion_visualization_) {
      RegisterFusionState(*computation_,
                          absl::StrCat("Fused |", producer->name(), "| into |",
                                       user->name(), "| inside FusionMerger"),
                          *consumer);
    }

    changed_ = true;
  }

  CHECK_EQ(0, producer->user_count()) << producer->ToString();
  TF_RETURN_IF_ERROR(computation_->RemoveInstruction(producer));
  TF_RETURN_IF_ERROR(cost_analysis_->RemoveInstruction(producer));
  fusion_info_cache_.Invalidate(producer);
  VLOG(2) << "Merged fusion instruction: " << producer->name()
          << " into users { "
          << absl::StrJoin(users, ", ",
                           [](std::string* out, HloInstruction* user) {
                             absl::StrAppend(out, user->name());
                           })
          << " }";
  return absl::OkStatus();
}

absl::Status FusionInstructionMerger::Run() {
  for (HloInstruction* producer : computation_->MakeInstructionPostOrder()) {
    if (producer->opcode() != HloOpcode::kFusion) {
      continue;
    }
    FusionDecision should_fuse = ShouldFuse(producer);
    if (should_fuse) {
      TF_RETURN_IF_ERROR(FuseIntoAllUsers(producer));
      ++total_merged_;
    } else {
      VLOG(3) << "Not fusing fusion |" << producer->name()
              << "| with all of it's users due to: " << should_fuse.Explain();
      if (dump_fusion_visualization_ && !producer->users().empty()) {
        RegisterFusionState(
            *computation_,
            absl::StrCat(
                "Not fusing fusion |", producer->name(),
                "| into all of its users due to: ", should_fuse.Explain()),
            // Just pick any consumer, since we are trying to merge into all.
            /*consumer=*/*producer->users()[0],
            /*producer=*/producer);
      }
    }
  }

  VLOG(1) << "FusionInstructionMerger EXIT"
          << " computation: " << computation_->name()
          << " total_visited: " << total_visited_
          << " total_merged: " << total_merged_ << " merge failures { "
          << " no_users: " << num_fail_no_users_
          << " not_loop_fusion: " << num_fail_not_loop_fusion_
          << " merge_all_users: " << num_fail_merge_all_users_
          << " uncoalesced_read: " << num_fail_uncoalesced_read_
          << " inefficient_fusion_emitter: "
          << num_fail_inefficient_fusion_emitter_
          << " slower_if_fused: " << num_fail_slower_if_fused_
          << " fusion_too_large: " << num_fail_fusion_too_large_ << " }";
  return absl::OkStatus();
}

bool TransposesMostData(const HloInstruction& fusion) {
  float score = 0;

  for (const HloInstruction* instr : fusion.fused_instructions()) {
    if (IsPhysicallyTransposing(*instr)) {
      score += 1.0 * ShapeUtil::ElementsInRecursive(instr->shape()) /
               ShapeUtil::ElementsInRecursive(fusion.shape());
      if (score >= 0.5) {
        VLOG(3) << fusion.ToString() << " transpose ratio exceeds " << score;
        return true;
      }
    }
  }

  return false;
}

FusionDecision FusionInstructionMerger::ShouldFuse(HloInstruction* producer) {
  ++total_visited_;

  VLOG(4) << "Considering producer " << producer->name();

  // Skip 'producer' instruction if there are no users into which we can
  // merge.
  if (producer->users().empty()) {
    ++num_fail_no_users_;
    return FusionDecision::Forbid("fusion has no users");
  }

  // Skip 'producer' instruction if it is not a loop fusion. Library fusion
  // instructions match specific patterns, so they shouldn't be further fused.
  // Input fusion instructions need to be rooted at a particular HLO (e.g.
  // kReduce), so they shouldn't be further fused either.
  if (!producer->IsLoopFusion()) {
    ++num_fail_not_loop_fusion_;
    return FusionDecision::Forbid("not a loop fusion");
  }

  auto producer_hero = GetRealHeroForMultiOutputFusion(*producer);

  bool has_reduction_user = false;
  for (const HloInstruction* user : producer->users()) {
    if (user->opcode() == HloOpcode::kBitcast) {
      ++num_fail_merge_all_users_;
      return FusionDecision::Forbid("not fusing bitcast ops");
    }
    if (user->IsCustomFusion()) {
      ++num_fail_merge_all_users_;
      return FusionDecision::Forbid("not fusing custom fusions");
    }
    auto consumer_hero = GetRealHeroForMultiOutputFusion(*user);
    if (auto compatible =
            FusionHeroesAreCompatible(producer_hero, consumer_hero);
        !compatible) {
      return compatible;
    }
    FusionDecision fusible = IsProducerConsumerFusible(*producer, *user);
    if (!fusible) {
      ++num_fail_merge_all_users_;
      VLOG(9) << user->ToString();
      return fusible;
    }
    if (IsInputFusibleReduction(*user)) {
      has_reduction_user = true;
    }
  }

  // We do not want to worsen reduction's memory access pattern by connecting
  // it to a producer which transposes most data.
  if (has_reduction_user && TransposesMostData(*producer)) {
    ++num_fail_uncoalesced_read_;
    return FusionDecision::Forbid("would read mostly uncoalesced");
  }

  for (const HloInstruction* user : producer->users()) {
    // Skip 'fusion' instruction if merging it into at least one of the users
    // would make the fusion use too much shared memory or registers.
    FusionDecision fits = FusionFitsInBudget(
        *user, *producer, gpu_device_info_,
        /*is_consumer_producer_fusion=*/true, &fusion_info_cache_);
    if (!fits) {
      ++num_fail_fusion_too_large_;
      return fits;
    }
  }

  if (!cost_analysis_) {
    VLOG(2) << "Running full HLO cost analysis for " << computation_->name();
    cost_analysis_.emplace(
        GpuHloCostAnalysis::Options{shape_size_function_,
                                    /*per_second_rates=*/{},
                                    /*min_latencies_seconds=*/{},
                                    /*count_multiple_input_accesses=*/true},
        gpu_device_info_);
    TF_CHECK_OK(computation_->Accept(&cost_analysis_.value()));
  }

  for (const HloInstruction* user : producer->users()) {
    if (cost_analysis_->ProducerConsumerMergedTooLarge(*producer, *user)) {
      ++num_fail_inefficient_fusion_emitter_;
      return FusionDecision::Forbid("if merged with ")
             << user->name() << " will generate huge IR";
    }
  }

  GpuPerformanceModel::RunTimes t = GpuPerformanceModel::EstimateRunTimes(
      producer, gpu_device_info_, &*cost_analysis_,
      GpuPerformanceModelOptions::Default(), producer->users());
  if (t.time_fused > t.time_unfused) {
    ++num_fail_slower_if_fused_;
    return FusionDecision::Forbid("will execute slower if fused");
  }

  return FusionDecision::Allow();
}

absl::StatusOr<bool> FusionMerger::Run(
    HloModule* module,
    const absl::flat_hash_set<absl::string_view>& execution_threads) {
  bool changed = false;
  VLOG(1) << "FusionMerger for module: " << module->name();
  for (auto* computation :
       module->MakeNonfusionComputations(execution_threads)) {
    VLOG(9) << "Before running FusionInstructionMerger for computation: "
            << computation->name();
    XLA_VLOG_LINES(9, computation->ToString());

    FusionInstructionMerger fusion_merger(computation, gpu_device_info_,
                                          shape_size_function_);
    TF_RETURN_IF_ERROR(fusion_merger.Run());
    changed |= fusion_merger.changed();

    VLOG(9) << "After running FusionInstructionMerger for computation: "
            << computation->name() << " changed: " << changed;
    XLA_VLOG_LINES(9, computation->ToString());
  }
  return changed;
}

}  // namespace gpu
}  // namespace xla

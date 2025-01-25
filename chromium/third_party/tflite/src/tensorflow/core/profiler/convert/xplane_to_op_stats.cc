/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/profiler/convert/xplane_to_op_stats.h"

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "tensorflow/core/profiler/convert/op_metrics_db_combiner.h"
#include "tensorflow/core/profiler/convert/step_events_to_steps_db.h"
#include "tensorflow/core/profiler/convert/xplane_to_kernel_stats_db.h"
#include "tensorflow/core/profiler/convert/xplane_to_op_metrics_db.h"
#include "tensorflow/core/profiler/convert/xplane_to_step_events.h"
#include "tensorflow/core/profiler/protobuf/diagnostics.pb.h"
#include "tensorflow/core/profiler/protobuf/hardware_types.pb.h"
#include "tensorflow/core/profiler/protobuf/op_metrics.pb.h"
#include "tensorflow/core/profiler/protobuf/op_stats.pb.h"
#include "tensorflow/core/profiler/protobuf/steps_db.pb.h"
#include "tensorflow/core/profiler/protobuf/tf_function.pb.h"
#include "tensorflow/core/profiler/utils/device_caps_utils.h"
#include "tensorflow/core/profiler/utils/event_span.h"
#include "tensorflow/core/profiler/utils/hardware_type_utils.h"
#include "tensorflow/core/profiler/utils/hlo_proto_map.h"
#include "tensorflow/core/profiler/utils/kernel_stats_utils.h"
#include "tensorflow/core/profiler/utils/math_utils.h"
#include "tensorflow/core/profiler/utils/xplane_schema.h"
#include "tensorflow/core/profiler/utils/xplane_utils.h"
#include "tensorflow/core/profiler/utils/xplane_visitor.h"
#include "tsl/profiler/protobuf/xplane.pb.h"
#include "tsl/profiler/utils/tf_xplane_visitor.h"
#include "tsl/profiler/utils/tpu_xplane_utils.h"
#include "tsl/profiler/utils/xplane_schema.h"

namespace tensorflow {
namespace profiler {
namespace {

using tsl::profiler::FindTensorCorePlanes;

std::string Hostname(const XSpace& space) {
  if (space.hostnames().empty()) return "localhost";
  DCHECK_EQ(space.hostnames_size(), 1);
  const std::string& hostname = space.hostnames(0);
  return hostname;
}

}  // namespace

PerfEnv MakePerfEnv(double peak_tera_flops_per_second,
                    std::vector<double> peak_bws) {
  PerfEnv result;
  result.set_peak_tera_flops_per_second(peak_tera_flops_per_second);

  for (const auto bw : peak_bws) {
    result.add_peak_bws_giga_bytes_per_second(bw);
  }
  result.set_ridge_point(tsl::profiler::TeraToGiga(peak_tera_flops_per_second) /
                         peak_bws[MemBwType::MEM_BW_TYPE_HBM_RW]);
  return result;
}

PerfEnv GetPerfEnvFromXPlane(const XPlane& device_plane) {
  DeviceCapabilities cap = GetDeviceCaps(device_plane);
  if (!absl::StartsWith(device_plane.name(), kTpuPlanePrefix)) {
    return MakePerfEnv(
        tsl::profiler::GigaToTera(GetFlopMaxThroughputPerSM(cap)) *
            cap.num_cores(),
        // Ideally, the cap should report separate hbm BW, for now set to same.
        {tsl::profiler::UniToGiga(cap.memory_bandwidth()),
         tsl::profiler::UniToGiga(cap.memory_bandwidth()),
         tsl::profiler::UniToGiga(cap.memory_bandwidth()),
         tsl::profiler::UniToGiga(cap.memory_bandwidth())});
  } else {
    XPlaneVisitor visitor = tsl::profiler::CreateTfXPlaneVisitor(&device_plane);
    auto peak_tera_flops_per_second =
        visitor.GetStat(StatType::kDevCapPeakTeraflopsPerSecond);
    auto peak_tera_flops_per_second_val =
        peak_tera_flops_per_second.has_value()
            ? peak_tera_flops_per_second->DoubleValue()
            : 0.0;
    auto peak_hbm_bw_giga_bytes_per_second =
        visitor.GetStat(StatType::kDevCapPeakHbmBwGigabytesPerSecond);
    auto peak_hbm_bw_giga_bytes_per_second_val =
        peak_hbm_bw_giga_bytes_per_second.has_value()
            ? peak_hbm_bw_giga_bytes_per_second->DoubleValue()
            : 0.0;
    auto peak_sram_rd_bw_giga_bytes_per_second =
        visitor.GetStat(StatType::kDevCapPeakSramRdBwGigabytesPerSecond);
    auto peak_sram_rd_bw_giga_bytes_per_second_val =
        peak_sram_rd_bw_giga_bytes_per_second.has_value()
            ? peak_sram_rd_bw_giga_bytes_per_second->DoubleValue()
            : 0.0;
    auto peak_sram_wr_bw_giga_bytes_per_second =
        visitor.GetStat(StatType::kDevCapPeakSramWrBwGigabytesPerSecond);
    auto peak_sram_wr_bw_giga_bytes_per_second_val =
        peak_sram_wr_bw_giga_bytes_per_second.has_value()
            ? peak_sram_wr_bw_giga_bytes_per_second->DoubleValue()
            : 0.0;
    return MakePerfEnv(peak_tera_flops_per_second_val,
                       {peak_hbm_bw_giga_bytes_per_second_val,
                        peak_sram_rd_bw_giga_bytes_per_second_val,
                        peak_sram_wr_bw_giga_bytes_per_second_val});
  }
}

void SetRunEnvironment(const XSpace& space, RunEnvironment* env) {
  // Currently, we only support profiling one host and one program.
  env->set_host_count(1);
  env->set_task_count(1);
  env->mutable_hostnames()->insert({Hostname(space), true});

  std::vector<const XPlane*> gpu_planes =
      FindPlanesWithPrefix(space, kGpuPlanePrefix);
  if (!gpu_planes.empty()) {
    absl::string_view gpu_model =
        GpuModelName(GetDeviceCaps(*gpu_planes.front()));
    if (!gpu_model.empty()) {
      env->set_device_type(std::string(gpu_model));
    } else {
      env->set_device_type("GPU");
    }
    env->set_device_core_count(gpu_planes.size());
  } else if (std::vector<const XPlane*> tpu_planes =
                 FindTensorCorePlanes(space);
             !tpu_planes.empty()) {
    XPlaneVisitor visitor =
        tsl::profiler::CreateTfXPlaneVisitor(tpu_planes.at(0));
    auto xstat = visitor.GetStat(StatType::kDeviceTypeString);
    if (xstat.has_value()) {
      env->set_device_type(std::string(xstat->StrOrRefValue()));
    }
    env->set_device_core_count(tpu_planes.size());
  } else {
    env->set_device_type("CPU");
    env->set_device_core_count(0);
  }
}

void PropagateXSpaceDiagnosticsToOpStats(const XSpace& space,
                                         OpStats* op_stats) {
  if (!space.errors().empty()) {
    absl::flat_hash_set<std::string> unique_errors;
    unique_errors.insert(space.errors().begin(), space.errors().end());
    *op_stats->mutable_diagnostics()->mutable_errors() = {unique_errors.begin(),
                                                          unique_errors.end()};
  }
  if (!space.warnings().empty()) {
    absl::flat_hash_set<std::string> unique_warnings;
    unique_warnings.insert(space.warnings().begin(), space.warnings().end());
    *op_stats->mutable_diagnostics()->mutable_warnings() = {
        unique_warnings.begin(), unique_warnings.end()};
  }
}

// This function should be idempotent to be called
void SetProgramIdToNameMap(const HloProtoMap& hlo_proto_map,
                           tensorflow::profiler::OpStats& op_stats) {
  auto& program_id_to_name_map = *op_stats.mutable_program_id_to_name_map();
  for (const auto& [program_id, hlo_proto] : hlo_proto_map) {
    program_id_to_name_map[program_id] = hlo_proto->hlo_module().name();
  }
}

OpStats ConvertXSpaceToOpStats(const XSpace& space,
                               const OpStatsOptions& options) {
  std::vector<const XPlane*> device_planes = FindTensorCorePlanes(space);
  bool is_tpu = !device_planes.empty();
  if (!is_tpu) {
    device_planes = FindPlanesWithPrefix(space, kGpuPlanePrefix);
  }
  OpStats op_stats;
  StepEvents step_events;
  PropagateXSpaceDiagnosticsToOpStats(space, &op_stats);
  // Convert device planes.
  OpMetricsDbCombiner op_metrics_db_combiner(
      op_stats.mutable_device_op_metrics_db());
  SetRunEnvironment(space, op_stats.mutable_run_environment());

  KernelReportMap reports;

  // TODO(b/161942993) parallelize XPlane processing per thread.
  for (const XPlane* device_trace : device_planes) {
    XPlane aggregated_xplane;
    bool use_aggregated_xplane = false;
    if (options.generate_op_metrics_db) {
      if (!op_stats.has_perf_env()) {
        *op_stats.mutable_perf_env() = GetPerfEnvFromXPlane(*device_trace);
      }
      if (!is_tpu) {
        OpMetricsDb device_op_metrics_db =
            ConvertDeviceTraceXPlaneToOpMetricsDb(*device_trace);
        op_metrics_db_combiner.Combine(device_op_metrics_db);
      } else {
        AggregateXPlane(*device_trace, aggregated_xplane);
        use_aggregated_xplane = true;
        OpMetricsDb device_op_metrics_db =
            ConvertTpuDeviceTraceXPlaneToOpMetricsDb(aggregated_xplane);
        op_metrics_db_combiner.Combine(device_op_metrics_db);
      }
    }
    if (options.generate_step_db) {
      StepEvents device_step_events = ConvertDeviceTraceXPlaneToStepEvents(
          use_aggregated_xplane ? aggregated_xplane : *device_trace);
      if (is_tpu) {
        // In TPU, we take the intersection of step events across cores as well
        // as hosts.see b/158249775 and cl/331842545.
        IntersectCombineStepEvents(device_step_events, &step_events);
      } else {
        UnionCombineStepEvents(device_step_events, &step_events);
      }
    }
    if (options.generate_kernel_stats_db) {
      ConvertDeviceTraceXPlaneToKernelReports(*device_trace,
                                              /*on_kernel_fn=*/{}, &reports);
    }
  }

  // Combine into reports.
  if (options.generate_kernel_stats_db) {
    CopyTopKDurationKernelReportsToDb(reports,
                                      op_stats.mutable_kernel_stats_db());
  }

  bool has_device = !device_planes.empty();
  // Convert a host plane.
  const XPlane* host_plane = FindPlaneWithName(space, kHostThreadsPlaneName);
  if (host_plane) {
    if (options.generate_op_metrics_db) {
      *op_stats.mutable_host_op_metrics_db() =
          ConvertHostThreadsXPlaneToOpMetricsDb(*host_plane);
    }
    if (options.generate_step_db && !has_device) {
      StepEvents host_step_events =
          ConvertHostThreadsXPlaneToStepEvents(*host_plane, nullptr);
      UnionCombineStepEvents(host_step_events, &step_events);
    }
    XPlaneVisitor visitor = tsl::profiler::CreateTfXPlaneVisitor(host_plane);
    auto stat = visitor.GetStat(StatType::kMatrixUnitUtilizationPercent);
    if (stat.has_value()) {
      op_stats.mutable_performance_counter_result()
          ->set_matrix_unit_utilization_percent(stat->DoubleValue());
    }
  }
  if (options.generate_step_db) {
    if (is_tpu) {
      // TPU steps relies on step number in step line in Xplane which has
      // already dropped the incomplete steps at both beginning and end.
      *op_stats.mutable_step_db() = ConvertStepEventsToStepDb(
          has_device, /*maybe_drop_incomplete_steps=*/false, step_events);
      *op_stats.mutable_device_op_metrics_db()->mutable_precision_stats() =
          ComputePrecisionStats(step_events);
    } else {
      StepEvents nonoverlapped_step_events =
          ToNonOverlappedStepEvents(step_events);
      *op_stats.mutable_step_db() = ConvertStepEventsToStepDb(
          has_device, options.maybe_drop_incomplete_steps,
          nonoverlapped_step_events);
      *op_stats.mutable_device_op_metrics_db()->mutable_precision_stats() =
          ComputePrecisionStats(nonoverlapped_step_events);
    }
  }

  // TODO(bvandermoon): Add the TPU equivalent for setting core details hostname
  if (!is_tpu) {
    CoreDetails& details =
        (*op_stats.mutable_core_id_to_details())[kDefaultGpuLocalCoreId];
    details.set_hostname(Hostname(space));
  }

  // Set program_id_to_name map in OpStats from Xspace
  // Will be non-op if the space does not have materialized device traces
  HloProtoMap hlo_proto_map;
  hlo_proto_map.AddHloProtosFromXSpace(space);
  SetProgramIdToNameMap(hlo_proto_map, op_stats);

  return op_stats;
}

}  // namespace profiler
}  // namespace tensorflow

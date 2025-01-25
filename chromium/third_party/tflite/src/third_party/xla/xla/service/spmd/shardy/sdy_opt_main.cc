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

#include "mlir/Dialect/Func/Extensions/AllExtensions.h"  // from @llvm-project
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/DialectRegistry.h"  // from @llvm-project
#include "mlir/InitAllPasses.h"  // from @llvm-project
#include "mlir/Tools/mlir-opt/MlirOptMain.h"  // from @llvm-project
#include "shardy/dialect/sdy/ir/dialect.h"  // from @shardy
#include "shardy/dialect/sdy/transforms/passes.h"  // from @shardy
#include "stablehlo/dialect/StablehloOps.h"  // from @stablehlo
#include "xla/mlir_hlo/mhlo/IR/hlo_ops.h"
#include "xla/mlir_hlo/mhlo/transforms/passes.h"
#include "xla/service/spmd/shardy/mhlo_round_trip/export_ops.h"
#include "xla/service/spmd/shardy/mhlo_round_trip/export_shardings.h"
#include "xla/service/spmd/shardy/mhlo_round_trip/mhlo_export.h"
#include "xla/service/spmd/shardy/mhlo_round_trip/mhlo_import.h"
#include "xla/service/spmd/shardy/mhlo_round_trip/shard_map_export.h"
#include "xla/service/spmd/shardy/round_trip_common/convert_sharding_custom_calls.h"
#include "xla/service/spmd/shardy/round_trip_common/identity_to_pass_through_while_args.h"
#include "xla/service/spmd/shardy/round_trip_common/import_constants.h"
#include "xla/service/spmd/shardy/round_trip_common/shard_map_import.h"
#include "xla/service/spmd/shardy/sdy_round_trip/export_ops.h"
#include "xla/service/spmd/shardy/sdy_round_trip/export_shardings.h"
#include "xla/service/spmd/shardy/sdy_round_trip/import_shardings.h"
#include "xla/service/spmd/shardy/sdy_round_trip/pipelines.h"
#include "xla/service/spmd/shardy/sdy_round_trip/test_utils/mhlo_to_hlo_to_mhlo.h"
#include "xla/service/spmd/shardy/sdy_round_trip/test_utils/testing_pipeline.h"

int main(int argc, char** argv) {
  mlir::registerAllPasses();
  mlir::mhlo::registerAllMhloPasses();

  mlir::DialectRegistry dialects;
  dialects.insert<mlir::func::FuncDialect, mlir::mhlo::MhloDialect,
                  mlir::sdy::SdyDialect, mlir::stablehlo::StablehloDialect>();
  mlir::func::registerAllExtensions(dialects);

  // Register all SDY passes and pipelines.
  mlir::sdy::registerAllSdyPassesAndPipelines();

  xla::sdy::registerMhloImportPipeline();
  xla::sdy::registerMhloImportShardingsPass();
  xla::sdy::registerShardMapImportPass();
  xla::sdy::registerConvertShardingCustomCallsPass();
  xla::sdy::registerAddIdentityToPassThroughWhileArgsPass();
  xla::sdy::registerImportConstantsPass();

  xla::sdy::registerMhloExportPipeline();
  xla::sdy::registerMhloExportShardingsPass();
  xla::sdy::registerShardMapExportPass();
  xla::sdy::registerExportOpsPass();

  xla::sdy::registerSdyRoundTripMhloToHloToMhloPass();
  xla::sdy::registerSdyRoundTripExportShardingsPass();
  xla::sdy::registerSdyRoundTripImportShardingsPass();
  xla::sdy::registerSdyRoundTripExportOpsPass();
  xla::sdy::registerSdyRoundTripExportPipeline();
  xla::sdy::registerSdyRoundTripImportPipeline();
  xla::sdy::registerSdyRoundTripTestingPipeline();

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "XLA SDY pass driver\n", dialects));
}

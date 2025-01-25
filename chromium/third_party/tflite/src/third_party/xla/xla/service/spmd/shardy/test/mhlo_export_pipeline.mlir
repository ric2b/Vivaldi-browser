// RUN: sdy_opt %s -xla-sdy-mhlo-export-pipeline 2>&1 | FileCheck %s

sdy.mesh @mesh_0 = <"axis_0"=2, "axis_1"=4, "axis_2"=4>
sdy.mesh @mesh_1 = <"axis_0"=16>
sdy.mesh @mesh_2 = <"x"=8, "y"=4>
sdy.mesh @mesh_3 = <"a"=2, "b"=2, "c"=2, "d"=2>
sdy.mesh @maximal_mesh_0 = <device_id=0>
sdy.mesh @maximal_mesh_1 = <device_id=1>

// CHECK-NOT: sdy.mesh

// CHECK-LABEL: func @non_trivial_common_mesh(
// CHECK-SAME:      %arg0: tensor<8x8xf32> {mhlo.sharding = "{devices=[4,8]<=[8,4]T(1,0)}"},
// CHECK-SAME:      %arg1: tensor<8x8xf32> {mhlo.sharding = "{devices=[1,2,16]<=[32] last_tile_dim_replicate}"},
// CHECK-SAME:      %arg2: tensor<8x16xf32> {mhlo.sharding = "{devices=[4,4,2]<=[2,16]T(1,0) last_tile_dim_replicate}"})
// CHECK-SAME:  -> tensor<8x16xf32> {
func.func @non_trivial_common_mesh(%arg0: tensor<8x8xf32> {sdy.sharding = #sdy.sharding<@mesh_0, [{"axis_2"}, {"axis_0", "axis_1"}]>},
                                   %arg1: tensor<8x8xf32> {sdy.sharding = #sdy.sharding<@mesh_0, [{}, {"axis_0"}]>},
                                   %arg2: tensor<8x16xf32> {sdy.sharding = #sdy.sharding<@mesh_0, [{"axis_1"}, {"axis_2"}]>}) -> tensor<8x16xf32> {
  %0 = mhlo.add %arg0, %arg1 : tensor<8x8xf32>
  %1 = "mhlo.dot" (%0, %arg2) : (tensor<8x8xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
  return %1 : tensor<8x16xf32>
}

// CHECK-LABEL: func @multiple_shardings(
// CHECK-SAME:      %arg0: tensor<8x8xf32> {mhlo.sharding = "{devices=[4,8]<=[8,4]T(1,0)}"},
// CHECK-SAME:      %arg1: tensor<8x8xf32> {mhlo.sharding = "{devices=[1,8,4]<=[2,4,4]T(0,2,1) last_tile_dim_replicate}"},
// CHECK-SAME:      %arg2: tensor<8x16xf32> {mhlo.sharding = "{devices=[1,4,8]<=[2,4,4]T(1,0,2) last_tile_dim_replicate}"})
// CHECK-SAME:  -> tensor<8x16xf32> {
func.func @multiple_shardings(%arg0: tensor<8x8xf32> {sdy.sharding = #sdy.sharding<@mesh_0, [{"axis_2"}, {"axis_0", "axis_1"}]>},
                              %arg1: tensor<8x8xf32> {sdy.sharding = #sdy.sharding<@mesh_0, [{}, {"axis_0", "axis_2"}]>},
                              %arg2: tensor<8x16xf32> {sdy.sharding = #sdy.sharding<@mesh_0, [{}, {"axis_1"}]>}) -> tensor<8x16xf32> {
// CHECK-NEXT: mhlo.add
// CHECK-SAME{LITERAL}: {mhlo.sharding = "{devices=[8,1,4]<=[2,4,4]T(1,0,2) last_tile_dim_replicate}"}
  %0 = mhlo.add %arg0, %arg1 {sdy.sharding = #sdy.sharding_per_value<[<@mesh_0, [{"axis_1","axis_0"}, {}]>]>} : tensor<8x8xf32>
  %1 = "mhlo.dot" (%0, %arg2) : (tensor<8x8xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
  return %1 : tensor<8x16xf32>
}

// CHECK-LABEL: func @single_axis(
// CHECK-SAME:      %arg0: tensor<32x8xf32> {mhlo.sharding = "{devices=[16,1]<=[16]}"},
// CHECK-SAME:      %arg1: tensor<8x16xf32>)
// CHECK-SAME:  -> tensor<32x16xf32> {
func.func @single_axis(%arg0: tensor<32x8xf32> {sdy.sharding = #sdy.sharding<@mesh_1, [{"axis_0"}, {}]>},
                       %arg1: tensor<8x16xf32>) -> tensor<32x16xf32> {
  %0 = "mhlo.dot" (%arg0, %arg1) : (tensor<32x8xf32>, tensor<8x16xf32>) -> tensor<32x16xf32>
  return %0 : tensor<32x16xf32>
}

// CHECK-LABEL: func @multi_result_op
func.func @multi_result_op(%arg0: tensor<4x64x8xf32>, %arg1: tensor<4x64x8xf32>) -> (tensor<4x8xf32>, tensor<4x8xf32>) {
  %0 = mhlo.constant dense<0.000000e+00> : tensor<f32>
// CHECK: mhlo.reduce
// CHECK-SAME{LITERAL}: {mhlo.sharding = "{{devices=[1,4,8]<=[8,4]T(1,0) last_tile_dim_replicate}, {devices=[4,1,8]<=[8,4]T(1,0) last_tile_dim_replicate}}"}
  %1:2 = mhlo.reduce(%arg0 init: %0), (%arg1 init: %0) across dimensions = [1]
    {sdy.sharding = #sdy.sharding_per_value<[<@mesh_2, [{}, {"y"}]>, <@mesh_2, [{"y"}, {}]>]>} :
    (tensor<4x64x8xf32>, tensor<4x64x8xf32>, tensor<f32>, tensor<f32>) -> (tensor<4x8xf32>, tensor<4x8xf32>)
    reducer(%arg2: tensor<f32>, %arg4: tensor<f32>) (%arg3: tensor<f32>, %arg5: tensor<f32>)  {
      %2 = mhlo.add %arg2, %arg4 : tensor<f32>
      %3 = mhlo.add %arg3, %arg5 : tensor<f32>
      mhlo.return %2, %3 : tensor<f32>, tensor<f32>
    }
  return %1#0, %1#1 : tensor<4x8xf32>, tensor<4x8xf32>
}

// CHECK-LABEL: func @fully_replicated(
// CHECK-SAME:      %arg0: tensor<8x8xf32> {mhlo.sharding = "{devices=[4,1,8]<=[8,4]T(1,0) last_tile_dim_replicate}"},
// CHECK-SAME:      %arg1: tensor<8x8xf32> {mhlo.sharding = "{replicated}"},
// CHECK-SAME:      %arg2: tensor<8x16xf32>)
// CHECK-SAME:  -> tensor<8x16xf32> {
func.func @fully_replicated(%arg0: tensor<8x8xf32> {sdy.sharding = #sdy.sharding<@mesh_2, [{"y"}, {}]>},
                            %arg1: tensor<8x8xf32> {sdy.sharding = #sdy.sharding<@mesh_2, [{}, {}]>},
                            %arg2: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = mhlo.add %arg0, %arg1 : tensor<8x8xf32>
  %1 = "mhlo.dot" (%0, %arg2) : (tensor<8x8xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
  return %1 : tensor<8x16xf32>
}

// CHECK-LABEL: func @split_axes(
// CHECK-SAME:      %arg0: tensor<8x8xf32> {mhlo.sharding = "{devices=[4,2,4]<=[2,2,2,4]T(3,1,0,2) last_tile_dim_replicate}"},
// CHECK-SAME:      %arg1: tensor<8x16xf32> {mhlo.sharding = "{devices=[2,4,4]<=[32] last_tile_dim_replicate}"})
// CHECK-SAME:  -> tensor<8x16xf32> {
func.func @split_axes(%arg0: tensor<8x8xf32> {sdy.sharding = #sdy.sharding<@mesh_2, [{"y"}, {"x":(2)2}]>},
                      %arg1: tensor<8x16xf32> {sdy.sharding = #sdy.sharding<@mesh_2, [{"x":(1)2}, {"x":(2)4}]>}) -> tensor<8x16xf32> {
// CHECK-NEXT: "mhlo.dot"
// CHECK-SAME{LITERAL}: {mhlo.sharding = "{devices=[4,1,8]<=[2,2,2,4]T(0,2,1,3) last_tile_dim_replicate}"}
  %1 = "mhlo.dot" (%arg0, %arg1) {sdy.sharding = #sdy.sharding_per_value<[<@mesh_2, [{"x":(1)2, "x":(4)2}, {}]>]>} : (tensor<8x8xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
  return %1 : tensor<8x16xf32>
}

// CHECK-LABEL: func @split_constants
func.func @split_constants() -> (tensor<8x8xf32>, tensor<8x8xf32>) {
  // CHECK-NEXT: %[[CONST_0:.*]] = mhlo.constant {mhlo.sharding = "{devices=[8,1,4]<=[32] last_tile_dim_replicate}"} dense<1.000000e+00>
  // CHECK-NEXT: %[[CONST_1:.*]] = mhlo.constant {mhlo.sharding = "{devices=[4,1,8]<=[8,4]T(1,0) last_tile_dim_replicate}"} dense<1.000000e+00>
  // CHECK-NEXT: return %[[CONST_0]], %[[CONST_1]]
  %0 = sdy.constant {sdy.sharding = #sdy.sharding_per_value<[<@mesh_2, [{"x"}, {}]>]>} dense<1.000000e+00> : tensor<8x8xf32>
  %1 = sdy.constant {sdy.sharding = #sdy.sharding_per_value<[<@mesh_2, [{"y"}, {}]>]>} dense<1.000000e+00> : tensor<8x8xf32>
  return %0, %1 : tensor<8x8xf32>, tensor<8x8xf32>
}

// CHECK-LABEL: func @reshard_all_closed
func.func @reshard_all_closed(%arg0: tensor<8x8xf32>) -> tensor<8x8xf32> {
  // CHECK: mhlo.copy %arg0 {mhlo.sharding = "{devices=[2,4,4]<=[2,4,4]T(0,2,1) last_tile_dim_replicate}"} :
  %0 = sdy.reshard %arg0 <@mesh_0, [{"axis_0"}, {"axis_2"}], replicated={"axis_1"}> : tensor<8x8xf32>
  return %0 : tensor<8x8xf32>
}

// CHECK-LABEL: func @reshard_partially_open_closed
func.func @reshard_partially_open_closed(%arg0: tensor<8x8xf32>) -> tensor<8x8xf32> {
  // CHECK: mhlo.copy %arg0 {mhlo.sharding = "{devices=[2,4,4]<=[2,4,4]T(0,2,1) last_tile_dim_replicate}"} :
  %0 = sdy.reshard %arg0 <@mesh_0, [{"axis_0", ?}, {"axis_2"}]> : tensor<8x8xf32>
  return %0 : tensor<8x8xf32>
}

// CHECK-LABEL: func @reshard_fully_open_partially_open
func.func @reshard_fully_open_partially_open(%arg0: tensor<8x8xf32>) -> tensor<8x8xf32> {
  // CHECK: mhlo.copy %arg0 {backend_config = "unspecified_dims=[1]", mhlo.sharding = "{devices=[2,1,16]<=[32] last_tile_dim_replicate}"}
  %0 = sdy.reshard %arg0 <@mesh_0, [{"axis_0", ?}, {?}]> : tensor<8x8xf32>
  return %0 : tensor<8x8xf32>
}

// CHECK-LABEL: func @sharding_in_manual_computation_body(
// CHECK-SAME:      %arg0: tensor<8x16xf32> {mhlo.sharding = "{devices=[2,2,4]<=[16] last_tile_dim_replicate}"},
// CHECK-SAME:      %arg1: tensor<16x32xf32> {mhlo.sharding = "{devices=[2,1,8]<=[2,2,4]T(1,0,2) last_tile_dim_replicate}"})
// CHECK-SAME:  -> (tensor<8x32xf32> {mhlo.sharding = "{devices=[2,1,8]<=[16] last_tile_dim_replicate}"}) {
func.func @sharding_in_manual_computation_body(%arg0: tensor<8x16xf32> {sdy.sharding = #sdy.sharding<@mesh_3, [{"a", ?}, {"b", ?}]>}, %arg1: tensor<16x32xf32> {sdy.sharding = #sdy.sharding<@mesh_3, [{"b", ?}, {?}]>}) -> (tensor<8x32xf32> {sdy.sharding = #sdy.sharding<@mesh_3, [{"a"}, {}]>}) {
// CHECK-NEXT: %0 = mhlo.copy %arg0 {mhlo.sharding = "{devices=[2,2,4]<=[2,2,4]T(1,0,2) last_tile_dim_replicate}"} : tensor<8x16xf32>
// CHECK-NEXT: %1 = mhlo.custom_call @SPMDFullToShardShape(%0) {mhlo.sharding = "{devices=[1,1,4,4]<=[16] last_tile_dims={manual, replicated}}"} : (tensor<8x16xf32>) -> tensor<4x8xf32>
// CHECK-NEXT: %2 = mhlo.copy %arg1 {mhlo.sharding = "{devices=[2,1,8]<=[2,2,4]T(1,0,2) last_tile_dim_replicate}"} : tensor<16x32xf32>
// CHECK-NEXT: %3 = mhlo.custom_call @SPMDFullToShardShape(%2) {mhlo.sharding = "{devices=[1,1,4,4]<=[16] last_tile_dims={manual, replicated}}"} : (tensor<16x32xf32>) -> tensor<8x32xf32>
// CHECK-NEXT: %4 = mhlo.copy %1 {mhlo.sharding = "{devices=[1,2,4,2]<=[8,2]T(1,0) last_tile_dims={manual, replicated}}"} : tensor<4x8xf32>
// CHECK-NEXT: %5 = mhlo.add %4, %4 {mhlo.sharding = "{devices=[2,1,4,2]<=[4,2,2]T(1,0,2) last_tile_dims={manual, replicated}}"} : tensor<4x8xf32>
// CHECK-NEXT: %6 = "mhlo.dot"(%5, %3) {mhlo.sharding = "{devices=[2,2,4]<=[4,4]T(1,0) last_tile_dims={manual}}"} : (tensor<4x8xf32>, tensor<8x32xf32>) -> tensor<4x32xf32>
// CHECK-NEXT: %7 = mhlo.sine %6 {mhlo.sharding = "{devices=[1,1,4,4]<=[16] last_tile_dims={manual, replicated}}"} : tensor<4x32xf32>
// CHECK-NEXT: %8 = mhlo.copy %7 {mhlo.sharding = "{devices=[1,1,4,4]<=[16] last_tile_dims={manual, replicated}}"} : tensor<4x32xf32>
// CHECK-NEXT: %9 = mhlo.custom_call @SPMDShardToFullShape(%8) {mhlo.sharding = "{devices=[2,1,8]<=[16] last_tile_dim_replicate}"} : (tensor<4x32xf32>) -> tensor<8x32xf32>
// CHECK-NEXT: return %9 : tensor<8x32xf32>
  %0 = sdy.manual_computation(%arg0, %arg1) in_shardings=[<@mesh_3, [{"b"}, {"a"}]>, <@mesh_3, [{"b"}, {}], replicated={"a"}>] out_shardings=[<@mesh_3, [{"a"}, {}], replicated={"b"}>] manual_axes={"a", "b"} (%arg2: tensor<4x8xf32>, %arg3: tensor<8x32xf32>) {
    %1 = sdy.reshard %arg2 <@mesh_3, [{}, {"d"}]> : tensor<4x8xf32>
    %2 = mhlo.add %1, %1 {sdy.sharding = #sdy.sharding_per_value<[<@mesh_3, [{"c"}, {}]>]>} : tensor<4x8xf32>
    %3 = "mhlo.dot"(%2, %arg3) {sdy.sharding = #sdy.sharding_per_value<[<@mesh_3, [{"c"}, {"d"}]>]>} : (tensor<4x8xf32>, tensor<8x32xf32>) -> tensor<4x32xf32>
    %4 = mhlo.sine %3 : tensor<4x32xf32>
    sdy.return %4 : tensor<4x32xf32>
  } : (tensor<8x16xf32>, tensor<16x32xf32>) -> tensor<8x32xf32>
  return %0 : tensor<8x32xf32>
}

// CHECK-LABEL: func @mesh_with_device_id_should_be_converted_to_maximal_sharding(%arg0: tensor<8x8xf32> {mhlo.sharding = "{maximal device=0}"}, %arg1: tensor<8x8xf32>)
func.func @mesh_with_device_id_should_be_converted_to_maximal_sharding(%arg0: tensor<8x8xf32> {sdy.sharding = #sdy.sharding<@maximal_mesh_0, [{}, {}]>}, %arg1: tensor<8x8xf32>) -> tensor<8x8xf32> {
    // CHECK: %[[ADD:.*]] = mhlo.add %arg0, %arg1
    %0 = mhlo.add %arg0, %arg1 : tensor<8x8xf32>
    // CHECK: %[[ADD_WITH_SHARDING:.*]] = mhlo.add %[[ADD]], %[[ADD]] {mhlo.sharding = "{maximal device=1}"}
    %1 = mhlo.add %0, %0 {sdy.sharding = #sdy.sharding_per_value<[<@maximal_mesh_1, [{}, {}]>]>} : tensor<8x8xf32>
    return %1 : tensor<8x8xf32>
}

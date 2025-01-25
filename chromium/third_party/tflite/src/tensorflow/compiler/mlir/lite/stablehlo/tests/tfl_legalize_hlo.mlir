// COM: This file is there to check that the `tfl-legalize-hlo` pass exists in `odml-to-stablehlo-opt`.

// RUN: odml-to-stablehlo-opt %s -tfl-legalize-hlo -split-input-file | FileCheck %s --dump-input=fail

// CHECK-LABEL: main
func.func @main(%arg0: tensor<5x7xf32>) -> tensor<5x7xf32> {
  func.return %arg0: tensor<5x7xf32>
}

// CHECK: return %arg0 : tensor<5x7xf32>

// -----

//===----------------------------------------------------------------------===//
// mhlo.transpose
//===----------------------------------------------------------------------===//

// CHECK-LABEL: transpose_2d
func.func @transpose_2d(%arg0: tensor<2x3xf32>) -> tensor<3x2xf32> {
  %0 = "mhlo.transpose"(%arg0) <{permutation = dense<[1, 0]> : tensor<2xi64>}> : (tensor<2x3xf32>) -> tensor<3x2xf32>
  func.return %0 : tensor<3x2xf32>
}

// CHECK-NEXT: %0 = "tfl.pseudo_const"() <{value = dense<[1, 0]> : tensor<2xi64>}> : () -> tensor<2xi64>
// CHECK-NEXT: %1 = "tfl.cast"(%0) : (tensor<2xi64>) -> tensor<2xi32>
// CHECK-NEXT: %2 = "tfl.transpose"(%arg0, %1) : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<3x2xf32>
// CHECK-NEXT: return %2 : tensor<3x2xf32>

// -----

// CHECK-LABEL: transpose_3d
func.func @transpose_3d(%arg0: tensor<1x2x3xf32>) -> tensor<3x2x1xf32> {
  %0 = "mhlo.transpose"(%arg0) <{permutation = dense<[2, 1, 0]> : tensor<3xi64>}> : (tensor<1x2x3xf32>) -> tensor<3x2x1xf32>
  func.return %0 : tensor<3x2x1xf32>
}

// CHECK-NEXT: %0 = "tfl.pseudo_const"() <{value = dense<[2, 1, 0]> : tensor<3xi64>}> : () -> tensor<3xi64>
// CHECK-NEXT: %1 = "tfl.cast"(%0) : (tensor<3xi64>) -> tensor<3xi32>
// CHECK-NEXT: %2 = "tfl.transpose"(%arg0, %1) : (tensor<1x2x3xf32>, tensor<3xi32>) -> tensor<3x2x1xf32>
// CHECK-NEXT: return %2 : tensor<3x2x1xf32>

// -----

// CHECK-LABEL: transpose_dynamic_2d
func.func @transpose_dynamic_2d(%arg0: tensor<?x4xf32>) -> tensor<4x?xf32> {
  %0 = "mhlo.transpose"(%arg0) <{permutation = dense<[1, 0]> : tensor<2xi64>}> : (tensor<?x4xf32>) -> tensor<4x?xf32>
  func.return %0 : tensor<4x?xf32>
}

// CHECK-NEXT: %0 = "tfl.pseudo_const"() <{value = dense<[1, 0]> : tensor<2xi64>}> : () -> tensor<2xi64>
// CHECK-NEXT: %1 = "tfl.cast"(%0) : (tensor<2xi64>) -> tensor<2xi32>
// CHECK-NEXT: %2 = "tfl.transpose"(%arg0, %1) : (tensor<?x4xf32>, tensor<2xi32>) -> tensor<4x?xf32>
// CHECK-NEXT: return %2 : tensor<4x?xf32>

// -----

//===----------------------------------------------------------------------===//
// mhlo.dot_general
//===----------------------------------------------------------------------===//

// CHECK-LABEL: dot_general
func.func @dot_general(%arg0: tensor<3x2x6x5x1xf32>, %arg1: tensor<3x2x4x6xf32>) -> tensor<3x5x1x4xf32> {
  %0 = "mhlo.dot_general"(%arg0, %arg1) {
    dot_dimension_numbers = #mhlo.dot<
      lhs_batching_dimensions = [0],
      lhs_contracting_dimensions = [1, 2],
      rhs_batching_dimensions = [0],
      rhs_contracting_dimensions = [1, 3]
    >,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>]
  } : (tensor<3x2x6x5x1xf32>, tensor<3x2x4x6xf32>) -> tensor<3x5x1x4xf32>
  func.return %0 : tensor<3x5x1x4xf32>
}

// CHECK:     %[[TRANSPOSED_0:.*]] = "tfl.transpose"
// CHECK:     %[[TRANSPOSED_1:.*]] = "tfl.transpose"
// CHECK-NEXT: %[[RESHAPED_0:.*]] = mhlo.reshape %[[TRANSPOSED_0]]
// CHECK-NEXT: %[[RESHAPED_1:.*]] = mhlo.reshape %[[TRANSPOSED_1]]
// CHECK-NEXT: %[[BMM_0:.*]] = "tfl.batch_matmul"(%[[RESHAPED_0]], %[[RESHAPED_1]]) <{adj_x = false, adj_y = false, asymmetric_quantize_inputs = false}> : (tensor<3x5x12xf32>, tensor<3x12x4xf32>) -> tensor<3x5x4xf32>
// CHECK-NEXT: %[[RESHAPED_BMM:.*]] = mhlo.reshape %[[BMM_0]]
// CHECK-NEXT: return %[[RESHAPED_BMM]] : tensor<3x5x1x4xf32>

// -----

// CHECK-LABEL: dot_general_repeated
func.func @dot_general_repeated(%arg0: tensor<1x1x1024xf32>, %arg1: tensor<1024x1024xf32>) -> tensor<1x1x1024xf32> {
  %0 = "mhlo.dot_general"(%arg0, %arg1) {
    dot_dimension_numbers = #mhlo.dot<
      lhs_batching_dimensions = [],
      lhs_contracting_dimensions = [2],
      rhs_batching_dimensions = [],
      rhs_contracting_dimensions = [0]
    >,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>]
  } : (tensor<1x1x1024xf32>, tensor<1024x1024xf32>) -> tensor<1x1x1024xf32>
  func.return %0 : tensor<1x1x1024xf32>
}

// CHECK:     %[[RESHAPED_0:.*]] = mhlo.reshape %arg0
// CHECK-NEXT: %[[RESHAPED_1:.*]] = mhlo.reshape %arg1
// CHECK-NEXT: %[[BMM_0:.*]] = "tfl.batch_matmul"(%[[RESHAPED_0]], %[[RESHAPED_1]]) <{adj_x = false, adj_y = false, asymmetric_quantize_inputs = false}> : {{.*}} -> tensor<1x1024xf32>
// CHECK-NEXT: %[[RESHAPED_BMM:.*]] = mhlo.reshape %[[BMM_0]]
// CHECK-NEXT: return %[[RESHAPED_BMM]] : tensor<1x1x1024xf32>

// -----

// CHECK-LABEL: dot_general_int8
func.func @dot_general_int8(%arg0: tensor<256xi8>, %arg1: tensor<256x8xi8>) -> tensor<8xi32> {
  %0 = "mhlo.dot_general"(%arg0, %arg1) {
    dot_dimension_numbers = #mhlo.dot<
      lhs_contracting_dimensions = [0],
      rhs_contracting_dimensions = [0]>,
      precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>]
  } : (tensor<256xi8>, tensor<256x8xi8>) -> tensor<8xi32>
  func.return %0 : tensor<8xi32>
}

// CHECK:      %[[RESHAPED_0:.*]] = mhlo.reshape %arg0
// CHECK-NEXT: %[[RESHAPED_1:.*]] = mhlo.reshape %arg1
// CHECK-NEXT: %[[BMM_0:.*]] = "tfl.batch_matmul"(%[[RESHAPED_0]], %[[RESHAPED_1]]) <{adj_x = false, adj_y = false, asymmetric_quantize_inputs = false}> : {{.*}} -> tensor<1x8xi32>
// CHECK-NEXT: %[[RESHAPED_BMM:.*]] = mhlo.reshape %[[BMM_0]]
// CHECK-NEXT: return %[[RESHAPED_BMM]] : tensor<8xi32>

// -----

// CHECK-LABEL: dot_general_dynamic_rhs_out_dim
func.func @dot_general_dynamic_rhs_out_dim(%arg0: tensor<4x4x256xf32>, %arg1: tensor<4x?x256xf32>) -> tensor<4x4x?xf32> {
  %0 = "mhlo.dot_general"(%arg0, %arg1) {
    dot_dimension_numbers = #mhlo.dot<
      lhs_batching_dimensions = [0],
      rhs_batching_dimensions = [0],
      lhs_contracting_dimensions = [2],
      rhs_contracting_dimensions = [2]
    >} : (tensor<4x4x256xf32>, tensor<4x?x256xf32>) -> tensor<4x4x?xf32>
  func.return %0 : tensor<4x4x?xf32>
}

// CHECK:      %0 = "tfl.pseudo_const"() <{value = dense<[0, 2, 1]> : tensor<3xi64>}> : () -> tensor<3xi64>
// CHECK-NEXT: %1 = "tfl.cast"(%0) : (tensor<3xi64>) -> tensor<3xi32>
// CHECK-NEXT: %2 = "tfl.transpose"(%arg1, %1) : (tensor<4x?x256xf32>, tensor<3xi32>) -> tensor<4x256x?xf32>
// CHECK-NEXT: %3 = mhlo.reshape %arg0 : (tensor<4x4x256xf32>) -> tensor<4x4x256xf32>
// CHECK-NEXT: %4 = "tfl.shape"(%arg1) : (tensor<4x?x256xf32>) -> tensor<3xi32>
// CHECK-NEXT: %5 = "tfl.pseudo_const"() <{value = dense<[-1, 0, -1]> : tensor<3xi32>}> : () -> tensor<3xi32>
// CHECK-NEXT: %6 = "tfl.pseudo_const"() <{value = dense<[-1, -1, 0]> : tensor<3xi32>}> : () -> tensor<3xi32>
// CHECK-NEXT: %7 = "tfl.pseudo_const"() <{value = dense<1> : tensor<i32>}> : () -> tensor<i32>
// CHECK-NEXT: %8 = "tfl.unsorted_segment_prod"(%4, %5, %7) : (tensor<3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %9 = "tfl.unsorted_segment_prod"(%4, %6, %7) : (tensor<3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %10 = "tfl.pseudo_const"() <{value = dense<4> : tensor<1xi32>}> : () -> tensor<1xi32>
// CHECK-NEXT: %11 = "tfl.concatenation"(%10, %9, %8) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<3xi32>
// CHECK-NEXT: %12 = mhlo.dynamic_reshape %2, %11 : (tensor<4x256x?xf32>, tensor<3xi32>) -> tensor<4x256x?xf32>
// CHECK-NEXT: %13 = "tfl.batch_matmul"(%3, %12) <{adj_x = false, adj_y = false, asymmetric_quantize_inputs = false}> : (tensor<4x4x256xf32>, tensor<4x256x?xf32>) -> tensor<4x4x?xf32>
// CHECK-NEXT: %14 = "tfl.shape"(%arg0) : (tensor<4x4x256xf32>) -> tensor<3xi32>
// CHECK-NEXT: %15 = "tfl.shape"(%arg1) : (tensor<4x?x256xf32>) -> tensor<3xi32>
// CHECK-NEXT: %16 = "tfl.pseudo_const"() <{value = dense<[0, 1]> : tensor<2xi64>}> : () -> tensor<2xi64>
// CHECK-NEXT: %17 = "tfl.gather"(%14, %16) <{axis = 0 : i32, batch_dims = 0 : i32}> : (tensor<3xi32>, tensor<2xi64>) -> tensor<2xi32>
// CHECK-NEXT: %18 = "tfl.pseudo_const"() <{value = dense<1> : tensor<1xi64>}> : () -> tensor<1xi64>
// CHECK-NEXT: %19 = "tfl.gather"(%15, %18) <{axis = 0 : i32, batch_dims = 0 : i32}> : (tensor<3xi32>, tensor<1xi64>) -> tensor<1xi32>
// CHECK-NEXT: %20 = "tfl.concatenation"(%17, %19) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<2xi32>, tensor<1xi32>) -> tensor<3xi32>
// CHECK-NEXT: %21 = mhlo.dynamic_reshape %13, %20 : (tensor<4x4x?xf32>, tensor<3xi32>) -> tensor<4x4x?xf32>
// CHECK-NEXT: return %21 : tensor<4x4x?xf32>

// -----

// CHECK-LABEL: dot_general_dynamic_batch_dim
func.func @dot_general_dynamic_batch_dim(%arg0: tensor<2x?x2x3xf32>, %arg1: tensor<2x?x4x3xf32>) -> tensor<2x?x2x4xf32> {
  %0 = "mhlo.dot_general"(%arg0, %arg1) {
    dot_dimension_numbers = #mhlo.dot<
      lhs_batching_dimensions = [0, 1],
      rhs_batching_dimensions = [0, 1],
      lhs_contracting_dimensions = [3],
      rhs_contracting_dimensions = [3]
    >} : (tensor<2x?x2x3xf32>, tensor<2x?x4x3xf32>) -> tensor<2x?x2x4xf32>
  func.return %0 : tensor<2x?x2x4xf32>
}

// CHECK:      %0 = "tfl.pseudo_const"() <{value = dense<[0, 1, 3, 2]> : tensor<4xi64>}> : () -> tensor<4xi64>
// CHECK-NEXT: %1 = "tfl.cast"(%0) : (tensor<4xi64>) -> tensor<4xi32>
// CHECK-NEXT: %2 = "tfl.transpose"(%arg1, %1) : (tensor<2x?x4x3xf32>, tensor<4xi32>) -> tensor<2x?x3x4xf32>
// CHECK-NEXT: %3 = "tfl.shape"(%arg0) : (tensor<2x?x2x3xf32>) -> tensor<4xi32>
// CHECK-NEXT: %4 = "tfl.pseudo_const"() <{value = dense<[-1, -1, 0, -1]> : tensor<4xi32>}> : () -> tensor<4xi32>
// CHECK-NEXT: %5 = "tfl.pseudo_const"() <{value = dense<[-1, -1, -1, 0]> : tensor<4xi32>}> : () -> tensor<4xi32>
// CHECK-NEXT: %6 = "tfl.pseudo_const"() <{value = dense<1> : tensor<i32>}> : () -> tensor<i32>
// CHECK-NEXT: %7 = "tfl.unsorted_segment_prod"(%3, %4, %6) : (tensor<4xi32>, tensor<4xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %8 = "tfl.unsorted_segment_prod"(%3, %5, %6) : (tensor<4xi32>, tensor<4xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %9 = "tfl.pseudo_const"() <{value = dense<[0, 1]> : tensor<2xi64>}> : () -> tensor<2xi64>
// CHECK-NEXT: %10 = "tfl.gather"(%3, %9) <{axis = 0 : i32, batch_dims = 0 : i32}> : (tensor<4xi32>, tensor<2xi64>) -> tensor<2xi32>
// CHECK-NEXT: %11 = "tfl.concatenation"(%10, %7, %8) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<2xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<4xi32>
// CHECK-NEXT: %12 = mhlo.dynamic_reshape %arg0, %11 : (tensor<2x?x2x3xf32>, tensor<4xi32>) -> tensor<2x?x2x3xf32>
// CHECK-NEXT: %13 = "tfl.shape"(%arg1) : (tensor<2x?x4x3xf32>) -> tensor<4xi32>
// CHECK-NEXT: %14 = "tfl.pseudo_const"() <{value = dense<[-1, -1, 0, -1]> : tensor<4xi32>}> : () -> tensor<4xi32>
// CHECK-NEXT: %15 = "tfl.pseudo_const"() <{value = dense<[-1, -1, -1, 0]> : tensor<4xi32>}> : () -> tensor<4xi32>
// CHECK-NEXT: %16 = "tfl.pseudo_const"() <{value = dense<1> : tensor<i32>}> : () -> tensor<i32>
// CHECK-NEXT: %17 = "tfl.unsorted_segment_prod"(%13, %14, %16) : (tensor<4xi32>, tensor<4xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %18 = "tfl.unsorted_segment_prod"(%13, %15, %16) : (tensor<4xi32>, tensor<4xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %19 = "tfl.pseudo_const"() <{value = dense<[0, 1]> : tensor<2xi64>}> : () -> tensor<2xi64>
// CHECK-NEXT: %20 = "tfl.gather"(%13, %19) <{axis = 0 : i32, batch_dims = 0 : i32}> : (tensor<4xi32>, tensor<2xi64>) -> tensor<2xi32>
// CHECK-NEXT: %21 = "tfl.concatenation"(%20, %18, %17) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<2xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<4xi32>
// CHECK-NEXT: %22 = mhlo.dynamic_reshape %2, %21 : (tensor<2x?x3x4xf32>, tensor<4xi32>) -> tensor<2x?x3x4xf32>
// CHECK-NEXT: %23 = "tfl.batch_matmul"(%12, %22) <{adj_x = false, adj_y = false, asymmetric_quantize_inputs = false}> : (tensor<2x?x2x3xf32>, tensor<2x?x3x4xf32>) -> tensor<2x?x2x4xf32>
// CHECK-NEXT: %24 = "tfl.shape"(%arg0) : (tensor<2x?x2x3xf32>) -> tensor<4xi32>
// CHECK-NEXT: %25 = "tfl.shape"(%arg1) : (tensor<2x?x4x3xf32>) -> tensor<4xi32>
// CHECK-NEXT: %26 = "tfl.pseudo_const"() <{value = dense<[0, 1, 2]> : tensor<3xi64>}> : () -> tensor<3xi64>
// CHECK-NEXT: %27 = "tfl.gather"(%24, %26) <{axis = 0 : i32, batch_dims = 0 : i32}> : (tensor<4xi32>, tensor<3xi64>) -> tensor<3xi32>
// CHECK-NEXT: %28 = "tfl.pseudo_const"() <{value = dense<2> : tensor<1xi64>}> : () -> tensor<1xi64>
// CHECK-NEXT: %29 = "tfl.gather"(%25, %28) <{axis = 0 : i32, batch_dims = 0 : i32}> : (tensor<4xi32>, tensor<1xi64>) -> tensor<1xi32>
// CHECK-NEXT: %30 = "tfl.concatenation"(%27, %29) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<3xi32>, tensor<1xi32>) -> tensor<4xi32>
// CHECK-NEXT: %31 = mhlo.dynamic_reshape %23, %30 : (tensor<2x?x2x4xf32>, tensor<4xi32>) -> tensor<2x?x2x4xf32>
// CHECK-NEXT: return %31 : tensor<2x?x2x4xf32>

// -----


// CHECK-LABEL: dot_general_dynamic_lhs_rhs_out_dims
func.func @dot_general_dynamic_lhs_rhs_out_dims(%arg0: tensor<2x2x?x3xf32>, %arg1: tensor<2x4x?x3xf32>) -> tensor<2x2x?x4x?xf32> {
  %0 = "mhlo.dot_general"(%arg0, %arg1) {
    dot_dimension_numbers = #mhlo.dot<
      lhs_batching_dimensions = [0],
      rhs_batching_dimensions = [0],
      lhs_contracting_dimensions = [3],
      rhs_contracting_dimensions = [3]
    >} : (tensor<2x2x?x3xf32>, tensor<2x4x?x3xf32>) -> tensor<2x2x?x4x?xf32>
  func.return %0 : tensor<2x2x?x4x?xf32>
}

// CHECK:      %0 = "tfl.pseudo_const"() <{value = dense<[0, 3, 1, 2]> : tensor<4xi64>}> : () -> tensor<4xi64>
// CHECK-NEXT: %1 = "tfl.cast"(%0) : (tensor<4xi64>) -> tensor<4xi32>
// CHECK-NEXT: %2 = "tfl.transpose"(%arg1, %1) : (tensor<2x4x?x3xf32>, tensor<4xi32>) -> tensor<2x3x4x?xf32>
// CHECK-NEXT: %3 = "tfl.shape"(%arg0) : (tensor<2x2x?x3xf32>) -> tensor<4xi32>
// CHECK-NEXT: %4 = "tfl.pseudo_const"() <{value = dense<[-1, 0, 0, -1]> : tensor<4xi32>}> : () -> tensor<4xi32>
// CHECK-NEXT: %5 = "tfl.pseudo_const"() <{value = dense<[-1, -1, -1, 0]> : tensor<4xi32>}> : () -> tensor<4xi32>
// CHECK-NEXT: %6 = "tfl.pseudo_const"() <{value = dense<1> : tensor<i32>}> : () -> tensor<i32>
// CHECK-NEXT: %7 = "tfl.unsorted_segment_prod"(%3, %4, %6) : (tensor<4xi32>, tensor<4xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %8 = "tfl.unsorted_segment_prod"(%3, %5, %6) : (tensor<4xi32>, tensor<4xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %9 = "tfl.pseudo_const"() <{value = dense<2> : tensor<1xi32>}> : () -> tensor<1xi32>
// CHECK-NEXT: %10 = "tfl.concatenation"(%9, %7, %8) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<3xi32>
// CHECK-NEXT: %11 = mhlo.dynamic_reshape %arg0, %10 : (tensor<2x2x?x3xf32>, tensor<3xi32>) -> tensor<2x?x3xf32>
// CHECK-NEXT: %12 = "tfl.shape"(%arg1) : (tensor<2x4x?x3xf32>) -> tensor<4xi32>
// CHECK-NEXT: %13 = "tfl.pseudo_const"() <{value = dense<[-1, 0, 0, -1]> : tensor<4xi32>}> : () -> tensor<4xi32>
// CHECK-NEXT: %14 = "tfl.pseudo_const"() <{value = dense<[-1, -1, -1, 0]> : tensor<4xi32>}> : () -> tensor<4xi32>
// CHECK-NEXT: %15 = "tfl.pseudo_const"() <{value = dense<1> : tensor<i32>}> : () -> tensor<i32>
// CHECK-NEXT: %16 = "tfl.unsorted_segment_prod"(%12, %13, %15) : (tensor<4xi32>, tensor<4xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %17 = "tfl.unsorted_segment_prod"(%12, %14, %15) : (tensor<4xi32>, tensor<4xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %18 = "tfl.pseudo_const"() <{value = dense<2> : tensor<1xi32>}> : () -> tensor<1xi32>
// CHECK-NEXT: %19 = "tfl.concatenation"(%18, %17, %16) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<3xi32>
// CHECK-NEXT: %20 = mhlo.dynamic_reshape %2, %19 : (tensor<2x3x4x?xf32>, tensor<3xi32>) -> tensor<2x3x?xf32>
// CHECK-NEXT: %21 = "tfl.batch_matmul"(%11, %20) <{adj_x = false, adj_y = false, asymmetric_quantize_inputs = false}> : (tensor<2x?x3xf32>, tensor<2x3x?xf32>) -> tensor<2x?x?xf32>
// CHECK-NEXT: %22 = "tfl.shape"(%arg0) : (tensor<2x2x?x3xf32>) -> tensor<4xi32>
// CHECK-NEXT: %23 = "tfl.shape"(%arg1) : (tensor<2x4x?x3xf32>) -> tensor<4xi32>
// CHECK-NEXT: %24 = "tfl.pseudo_const"() <{value = dense<[0, 1, 2]> : tensor<3xi64>}> : () -> tensor<3xi64>
// CHECK-NEXT: %25 = "tfl.gather"(%22, %24) <{axis = 0 : i32, batch_dims = 0 : i32}> : (tensor<4xi32>, tensor<3xi64>) -> tensor<3xi32>
// CHECK-NEXT: %26 = "tfl.pseudo_const"() <{value = dense<[1, 2]> : tensor<2xi64>}> : () -> tensor<2xi64>
// CHECK-NEXT: %27 = "tfl.gather"(%23, %26) <{axis = 0 : i32, batch_dims = 0 : i32}> : (tensor<4xi32>, tensor<2xi64>) -> tensor<2xi32>
// CHECK-NEXT: %28 = "tfl.concatenation"(%25, %27) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<3xi32>, tensor<2xi32>) -> tensor<5xi32>
// CHECK-NEXT: %29 = mhlo.dynamic_reshape %21, %28 : (tensor<2x?x?xf32>, tensor<5xi32>) -> tensor<2x2x?x4x?xf32>
// CHECK-NEXT: return %29 : tensor<2x2x?x4x?xf32>

// -----

// CHECK-LABEL: dot_general_dynamic_contracting_dim
func.func @dot_general_dynamic_contracting_dim(%arg0: tensor<4x4x?xf32>, %arg1: tensor<4x?x256xf32>) -> tensor<4x4x256xf32> {
  %0 = "mhlo.dot_general"(%arg0, %arg1) {
    dot_dimension_numbers = #mhlo.dot<
      lhs_batching_dimensions = [0],
      rhs_batching_dimensions = [0],
      lhs_contracting_dimensions = [2],
      rhs_contracting_dimensions = [1]
    >} : (tensor<4x4x?xf32>, tensor<4x?x256xf32>) -> tensor<4x4x256xf32>
  func.return %0 : tensor<4x4x256xf32>
}

// CHECK:      %0 = "tfl.shape"(%arg0) : (tensor<4x4x?xf32>) -> tensor<3xi32>
// CHECK-NEXT: %1 = "tfl.pseudo_const"() <{value = dense<[-1, 0, -1]> : tensor<3xi32>}> : () -> tensor<3xi32>
// CHECK-NEXT: %2 = "tfl.pseudo_const"() <{value = dense<[-1, -1, 0]> : tensor<3xi32>}> : () -> tensor<3xi32>
// CHECK-NEXT: %3 = "tfl.pseudo_const"() <{value = dense<1> : tensor<i32>}> : () -> tensor<i32>
// CHECK-NEXT: %4 = "tfl.unsorted_segment_prod"(%0, %1, %3) : (tensor<3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %5 = "tfl.unsorted_segment_prod"(%0, %2, %3) : (tensor<3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %6 = "tfl.pseudo_const"() <{value = dense<4> : tensor<1xi32>}> : () -> tensor<1xi32>
// CHECK-NEXT: %7 = "tfl.concatenation"(%6, %4, %5) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<3xi32>
// CHECK-NEXT: %8 = mhlo.dynamic_reshape %arg0, %7 : (tensor<4x4x?xf32>, tensor<3xi32>) -> tensor<4x4x?xf32>
// CHECK-NEXT: %9 = "tfl.shape"(%arg1) : (tensor<4x?x256xf32>) -> tensor<3xi32>
// CHECK-NEXT: %10 = "tfl.pseudo_const"() <{value = dense<[-1, -1, 0]> : tensor<3xi32>}> : () -> tensor<3xi32>
// CHECK-NEXT: %11 = "tfl.pseudo_const"() <{value = dense<[-1, 0, -1]> : tensor<3xi32>}> : () -> tensor<3xi32>
// CHECK-NEXT: %12 = "tfl.pseudo_const"() <{value = dense<1> : tensor<i32>}> : () -> tensor<i32>
// CHECK-NEXT: %13 = "tfl.unsorted_segment_prod"(%9, %10, %12) : (tensor<3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %14 = "tfl.unsorted_segment_prod"(%9, %11, %12) : (tensor<3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<1xi32>
// CHECK-NEXT: %15 = "tfl.pseudo_const"() <{value = dense<4> : tensor<1xi32>}> : () -> tensor<1xi32>
// CHECK-NEXT: %16 = "tfl.concatenation"(%15, %14, %13) <{axis = 0 : i32, fused_activation_function = "NONE"}> : (tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<3xi32>
// CHECK-NEXT: %17 = mhlo.dynamic_reshape %arg1, %16 : (tensor<4x?x256xf32>, tensor<3xi32>) -> tensor<4x?x256xf32>
// CHECK-NEXT: %18 = "tfl.batch_matmul"(%8, %17) <{adj_x = false, adj_y = false, asymmetric_quantize_inputs = false}> : (tensor<4x4x?xf32>, tensor<4x?x256xf32>) -> tensor<4x4x256xf32>
// CHECK-NEXT: %19 = mhlo.reshape %18 : (tensor<4x4x256xf32>) -> tensor<4x4x256xf32>
// CHECK-NEXT: return %19 : tensor<4x4x256xf32>

// -----

//===----------------------------------------------------------------------===//
// mhlo.reduce
//===----------------------------------------------------------------------===//

// CHECK-LABEL: argmax
func.func @argmax(%arg0: tensor<4x32x256xf32>) -> (tensor<4x32xf32>, tensor<4x32xi32>) {
  %0 = mhlo.constant dense<0xFF800000> : tensor<f32>
  %1 = mhlo.constant dense<0> : tensor<i32>
  %2 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<256xi32>
  %3 = "mhlo.broadcast_in_dim"(%2) <{broadcast_dimensions = dense<2> : tensor<1xi64>}> : (tensor<256xi32>) -> tensor<4x32x256xi32>
  %4:2 = "mhlo.reduce"(%arg0, %3, %0, %1) ({
  ^bb0(%arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<f32>, %arg4: tensor<i32>):
    %7 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction GT>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %8 = "mhlo.compare"(%arg1, %arg1) {comparison_direction = #mhlo<comparison_direction NE>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %9 = mhlo.or %7, %8 : tensor<i1>
    %10 = "mhlo.select"(%9, %arg1, %arg3) : (tensor<i1>, tensor<f32>, tensor<f32>) -> tensor<f32>
    %11 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction EQ>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %12 = "mhlo.compare"(%arg2, %arg4) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %13 = mhlo.and %11, %12 : tensor<i1>
    %14 = mhlo.or %9, %13 : tensor<i1>
    %15 = "mhlo.select"(%14, %arg2, %arg4) : (tensor<i1>, tensor<i32>, tensor<i32>) -> tensor<i32>
    "mhlo.return"(%10, %15) : (tensor<f32>, tensor<i32>) -> ()
  }) {dimensions = dense<2> : tensor<1xi64>} : (tensor<4x32x256xf32>, tensor<4x32x256xi32>, tensor<f32>, tensor<i32>) -> (tensor<4x32xf32>, tensor<4x32xi32>)
  func.return %4#0, %4#1 : tensor<4x32xf32>, tensor<4x32xi32>
}

// CHECK:     %0 = mhlo.constant dense<0xFF800000> : tensor<f32>
// CHECK-DAG: %1 = mhlo.constant dense<0> : tensor<i32>
// CHECK:     %2 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<256xi32>
// CHECK:     %3 = "mhlo.broadcast_in_dim"(%2) <{broadcast_dimensions = dense<2> : tensor<1xi64>}> : (tensor<256xi32>) -> tensor<4x32x256xi32>
// CHECK:     %cst = arith.constant dense<2> : tensor<1xi32>
// CHECK:     %4 = "tfl.reduce_max"(%arg0, %cst) <{keep_dims = false}> : (tensor<4x32x256xf32>, tensor<1xi32>) -> tensor<4x32xf32>
// CHECK:     %5 = "tfl.arg_max"(%arg0, %cst) : (tensor<4x32x256xf32>, tensor<1xi32>) -> tensor<4x32xi32>
// CHECK:     return %4, %5 : tensor<4x32xf32>, tensor<4x32xi32>

// -----

// CHECK-LABEL: argmax_constant
func.func @argmax_constant(%arg0: tensor<2x2x4xf32>) -> (tensor<2x2xf32>, tensor<2x2xi32>) {
  %0 = mhlo.constant dense<0xFF800000> : tensor<f32>
  %1 = mhlo.constant dense<0> : tensor<i32>
  %3 = mhlo.constant dense<[[[0, 1, 2, 3], [0, 1, 2, 3]], [[0, 1, 2, 3], [0, 1, 2, 3]]]> : tensor<2x2x4xi32>
  %4:2 = "mhlo.reduce"(%arg0, %3, %0, %1) ({
  ^bb0(%arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<f32>, %arg4: tensor<i32>):
    %7 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction GT>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %8 = "mhlo.compare"(%arg1, %arg1) {comparison_direction = #mhlo<comparison_direction NE>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %9 = mhlo.or %7, %8 : tensor<i1>
    %10 = "mhlo.select"(%9, %arg1, %arg3) : (tensor<i1>, tensor<f32>, tensor<f32>) -> tensor<f32>
    %11 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction EQ>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %12 = "mhlo.compare"(%arg2, %arg4) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %13 = mhlo.and %11, %12 : tensor<i1>
    %14 = mhlo.or %9, %13 : tensor<i1>
    %15 = "mhlo.select"(%14, %arg2, %arg4) : (tensor<i1>, tensor<i32>, tensor<i32>) -> tensor<i32>
    "mhlo.return"(%10, %15) : (tensor<f32>, tensor<i32>) -> ()
  }) {dimensions = dense<2> : tensor<1xi64>} : (tensor<2x2x4xf32>, tensor<2x2x4xi32>, tensor<f32>, tensor<i32>) -> (tensor<2x2xf32>, tensor<2x2xi32>)
  func.return %4#0, %4#1 : tensor<2x2xf32>, tensor<2x2xi32>
}

// CHECK-DAG: %0 = mhlo.constant dense<0xFF800000> : tensor<f32>
// CHECK-DAG: %1 = mhlo.constant dense<0> : tensor<i32>
// CHECK:     %2 = mhlo.constant dense<{{\[\[}}[0, 1, 2, 3], [0, 1, 2, 3]], {{\[\[}}0, 1, 2, 3], [0, 1, 2, 3]]]> : tensor<2x2x4xi32>
// CHECK:     %cst = arith.constant dense<2> : tensor<1xi32>
// CHECK:     %3 = "tfl.reduce_max"(%arg0, %cst) <{keep_dims = false}> : (tensor<2x2x4xf32>, tensor<1xi32>) -> tensor<2x2xf32>
// CHECK:     %4 = "tfl.arg_max"(%arg0, %cst) : (tensor<2x2x4xf32>, tensor<1xi32>) -> tensor<2x2xi32>
// CHECK:     return %3, %4 : tensor<2x2xf32>, tensor<2x2xi32>

// -----

// CHECK-LABEL: argmax_constant_non_z_axis
func.func @argmax_constant_non_z_axis(%arg0: tensor<4x4xf32>) -> (tensor<4xf32>, tensor<4xi32>) {
  %0 = mhlo.constant dense<0xFF800000> : tensor<f32>
  %1 = mhlo.constant dense<0> : tensor<i32>
  %3 = mhlo.constant dense<[[0, 0, 0, 0], [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3]]> : tensor<4x4xi32>
  %4:2 = "mhlo.reduce"(%arg0, %3, %0, %1) ({
  ^bb0(%arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<f32>, %arg4: tensor<i32>):
    %7 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction GT>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %8 = "mhlo.compare"(%arg1, %arg1) {comparison_direction = #mhlo<comparison_direction NE>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %9 = mhlo.or %7, %8 : tensor<i1>
    %10 = "mhlo.select"(%9, %arg1, %arg3) : (tensor<i1>, tensor<f32>, tensor<f32>) -> tensor<f32>
    %11 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction EQ>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %12 = "mhlo.compare"(%arg2, %arg4) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %13 = mhlo.and %11, %12 : tensor<i1>
    %14 = mhlo.or %9, %13 : tensor<i1>
    %15 = "mhlo.select"(%14, %arg2, %arg4) : (tensor<i1>, tensor<i32>, tensor<i32>) -> tensor<i32>
    "mhlo.return"(%10, %15) : (tensor<f32>, tensor<i32>) -> ()
  }) {dimensions = dense<0> : tensor<1xi64>} : (tensor<4x4xf32>, tensor<4x4xi32>, tensor<f32>, tensor<i32>) -> (tensor<4xf32>, tensor<4xi32>)
  func.return %4#0, %4#1 : tensor<4xf32>, tensor<4xi32>
}

// CHECK-DAG: %0 = mhlo.constant dense<0xFF800000> : tensor<f32>
// CHECK-DAG: %1 = mhlo.constant dense<0> : tensor<i32>
// CHECK:     %2 = mhlo.constant dense<{{\[\[}}0, 0, 0, 0], [1, 1, 1, 1], [2, 2, 2, 2], [3, 3, 3, 3]]> : tensor<4x4xi32>
// CHECK:     %cst = arith.constant dense<0> : tensor<1xi32>
// CHECK:     %3 = "tfl.reduce_max"(%arg0, %cst) <{keep_dims = false}> : (tensor<4x4xf32>, tensor<1xi32>) -> tensor<4xf32>
// CHECK:     %4 = "tfl.arg_max"(%arg0, %cst) : (tensor<4x4xf32>, tensor<1xi32>) -> tensor<4xi32>
// CHECK:     return %3, %4 : tensor<4xf32>, tensor<4xi32>

// -----

// CHECK-LABEL: argmax_bool
func.func @argmax_bool(%arg0: tensor<2xi1>) -> tensor<i32> {
  %0 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<2xi32>
  %1 = mhlo.constant dense<false> : tensor<i1>
  %2 = mhlo.constant dense<0> : tensor<i32>
  %3:2 = mhlo.reduce(%arg0 init: %1), (%0 init: %2) across dimensions = [0] : (tensor<2xi1>, tensor<2xi32>, tensor<i1>, tensor<i32>) -> (tensor<i1>, tensor<i32>)
    reducer(%arg1: tensor<i1>, %arg3: tensor<i1>) (%arg2: tensor<i32>, %arg4: tensor<i32>)  {
    %4 = mhlo.compare  GT, %arg1, %arg3 : (tensor<i1>, tensor<i1>) -> tensor<i1>
    %5 = mhlo.select %4, %arg1, %arg3 : tensor<i1>, tensor<i1>
    %6 = mhlo.compare  EQ, %arg1, %arg3 : (tensor<i1>, tensor<i1>) -> tensor<i1>
    %7 = mhlo.compare  LT, %arg2, %arg4 : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %8 = mhlo.and %6, %7 : tensor<i1>
    %9 = mhlo.or %4, %8 : tensor<i1>
    %10 = mhlo.select %9, %arg2, %arg4 : tensor<i1>, tensor<i32>
    mhlo.return %5, %10 : tensor<i1>, tensor<i32>
  }
  return %3#1 : tensor<i32>
}

// CHECK:     %0 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<2xi32>
// CHECK-DAG: %1 = mhlo.constant dense<false> : tensor<i1>
// CHECK:     %2 = mhlo.constant dense<0> : tensor<i32>
// CHECK:     %cst = arith.constant dense<0> : tensor<1xi32>
// CHECK:     %3 = "tfl.reduce_any"(%arg0, %cst) <{keep_dims = false}> : (tensor<2xi1>, tensor<1xi32>) -> tensor<i1>
// CHECK:     %4 = "tfl.arg_max"(%arg0, %cst) : (tensor<2xi1>, tensor<1xi32>) -> tensor<i32>
// CHECK:     return %4 : tensor<i32>

// -----

// CHECK-LABEL: argmin
func.func @argmin(%arg0: tensor<4x32x256xf32>) -> (tensor<4x32xf32>, tensor<4x32xi32>) {
  %0 = mhlo.constant dense<0x7F800000> : tensor<f32>
  %1 = mhlo.constant dense<0> : tensor<i32>
  %2 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<256xi32>
  %3 = "mhlo.broadcast_in_dim"(%2) <{broadcast_dimensions = dense<2> : tensor<1xi64>}> : (tensor<256xi32>) -> tensor<4x32x256xi32>
  %4:2 = "mhlo.reduce"(%arg0, %3, %0, %1) ({
  ^bb0(%arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<f32>, %arg4: tensor<i32>):
    %7 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %8 = "mhlo.compare"(%arg1, %arg1) {comparison_direction = #mhlo<comparison_direction NE>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %9 = mhlo.or %7, %8 : tensor<i1>
    %10 = "mhlo.select"(%9, %arg1, %arg3) : (tensor<i1>, tensor<f32>, tensor<f32>) -> tensor<f32>
    %11 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction EQ>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %12 = "mhlo.compare"(%arg2, %arg4) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %13 = mhlo.and %11, %12 : tensor<i1>
    %14 = mhlo.or %9, %13 : tensor<i1>
    %15 = "mhlo.select"(%14, %arg2, %arg4) : (tensor<i1>, tensor<i32>, tensor<i32>) -> tensor<i32>
    "mhlo.return"(%10, %15) : (tensor<f32>, tensor<i32>) -> ()
  }) {dimensions = dense<2> : tensor<1xi64>} : (tensor<4x32x256xf32>, tensor<4x32x256xi32>, tensor<f32>, tensor<i32>) -> (tensor<4x32xf32>, tensor<4x32xi32>)
  func.return %4#0, %4#1 : tensor<4x32xf32>, tensor<4x32xi32>
}

// CHECK-DAG: %0 = mhlo.constant dense<0x7F800000> : tensor<f32>
// CHECK:     %1 = mhlo.constant dense<0> : tensor<i32>
// CHECK:     %2 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<256xi32>
// CHECK:     %3 = "mhlo.broadcast_in_dim"(%2) <{broadcast_dimensions = dense<2> : tensor<1xi64>}> : (tensor<256xi32>) -> tensor<4x32x256xi32>
// CHECK:     %cst = arith.constant dense<2> : tensor<1xi32>
// CHECK:     %4 = "tfl.reduce_min"(%arg0, %cst) <{keep_dims = false}> : (tensor<4x32x256xf32>, tensor<1xi32>) -> tensor<4x32xf32>
// CHECK:     %5 = "tfl.arg_min"(%arg0, %cst) : (tensor<4x32x256xf32>, tensor<1xi32>) -> tensor<4x32xi32>
// CHECK:     return %4, %5 : tensor<4x32xf32>, tensor<4x32xi32>

// -----

// CHECK-LABEL: argmin_i16
func.func @argmin_i16(%arg0: tensor<2xi16>) -> (tensor<i16>, tensor<i32>) {
  %0 = mhlo.constant dense<false> : tensor<i1>
  %1 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<2xi32>
  %2 = mhlo.constant dense<32767> : tensor<i16>
  %3 = mhlo.constant dense<0> : tensor<i32>
  %4:2 = "mhlo.reduce"(%arg0, %1, %2, %3) ({
  ^bb0(%arg1: tensor<i16>, %arg2: tensor<i32>, %arg3: tensor<i16>, %arg4: tensor<i32>):
    %11 = mhlo.constant dense<false> : tensor<i1>
    %12 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<i16>, tensor<i16>) -> tensor<i1>
    %13 = "mhlo.select"(%12, %arg1, %arg3) : (tensor<i1>, tensor<i16>, tensor<i16>) -> tensor<i16>
    %14 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction EQ>} : (tensor<i16>, tensor<i16>) -> tensor<i1>
    %15 = "mhlo.compare"(%arg2, %arg4) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %16 = mhlo.and %14, %15 : tensor<i1>
    %17 = mhlo.or %12, %16 : tensor<i1>
    %18 = "mhlo.select"(%17, %arg2, %arg4) : (tensor<i1>, tensor<i32>, tensor<i32>) -> tensor<i32>
    "mhlo.return"(%13, %18) : (tensor<i16>, tensor<i32>) -> ()
  }) {dimensions = dense<0> : tensor<1xi64>} : (tensor<2xi16>, tensor<2xi32>, tensor<i16>, tensor<i32>) -> (tensor<i16>, tensor<i32>)
  func.return %4#0, %4#1 : tensor<i16>, tensor<i32>
}

// CHECK:     %0 = mhlo.constant dense<false> : tensor<i1>
// CHECK:     %1 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<2xi32>
// CHECK-DAG: %2 = mhlo.constant dense<32767> : tensor<i16>
// CHECK:     %3 = mhlo.constant dense<0> : tensor<i32>
// CHECK:     %cst = arith.constant dense<0> : tensor<1xi32>
// CHECK:     %4 = "tfl.reduce_min"(%arg0, %cst) <{keep_dims = false}> : (tensor<2xi16>, tensor<1xi32>) -> tensor<i16>
// CHECK:     %5 = "tfl.arg_min"(%arg0, %cst) : (tensor<2xi16>, tensor<1xi32>) -> tensor<i32>
// CHECK:     return %4, %5 : tensor<i16>, tensor<i32>

// -----

// CHECK-LABEL: argmin_constant
func.func @argmin_constant(%arg0: tensor<2x2x4xf32>) -> (tensor<2x2xf32>, tensor<2x2xi32>) {
  %0 = mhlo.constant dense<0x7F800000> : tensor<f32>
  %1 = mhlo.constant dense<0> : tensor<i32>
  %3 = mhlo.constant dense<[[[0, 1, 2, 3], [0, 1, 2, 3]], [[0, 1, 2, 3], [0, 1, 2, 3]]]> : tensor<2x2x4xi32>
  %4:2 = "mhlo.reduce"(%arg0, %3, %0, %1) ({
  ^bb0(%arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<f32>, %arg4: tensor<i32>):
    %7 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %8 = "mhlo.compare"(%arg1, %arg1) {comparison_direction = #mhlo<comparison_direction NE>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %9 = mhlo.or %7, %8 : tensor<i1>
    %10 = "mhlo.select"(%9, %arg1, %arg3) : (tensor<i1>, tensor<f32>, tensor<f32>) -> tensor<f32>
    %11 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction EQ>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %12 = "mhlo.compare"(%arg2, %arg4) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %13 = mhlo.and %11, %12 : tensor<i1>
    %14 = mhlo.or %9, %13 : tensor<i1>
    %15 = "mhlo.select"(%14, %arg2, %arg4) : (tensor<i1>, tensor<i32>, tensor<i32>) -> tensor<i32>
    "mhlo.return"(%10, %15) : (tensor<f32>, tensor<i32>) -> ()
  }) {dimensions = dense<2> : tensor<1xi64>} : (tensor<2x2x4xf32>, tensor<2x2x4xi32>, tensor<f32>, tensor<i32>) -> (tensor<2x2xf32>, tensor<2x2xi32>)
  func.return %4#0, %4#1 : tensor<2x2xf32>, tensor<2x2xi32>
}

// CHECK-DAG: %0 = mhlo.constant dense<0x7F800000> : tensor<f32>
// CHECK-DAG: %1 = mhlo.constant dense<0> : tensor<i32>
// CHECK:     %2 = mhlo.constant dense<{{\[\[}}[0, 1, 2, 3], [0, 1, 2, 3]], {{\[\[}}0, 1, 2, 3], [0, 1, 2, 3]]]> : tensor<2x2x4xi32>
// CHECK:     %cst = arith.constant dense<2> : tensor<1xi32>
// CHECK:     %3 = "tfl.reduce_min"(%arg0, %cst) <{keep_dims = false}> : (tensor<2x2x4xf32>, tensor<1xi32>) -> tensor<2x2xf32>
// CHECK:     %4 = "tfl.arg_min"(%arg0, %cst) : (tensor<2x2x4xf32>, tensor<1xi32>) -> tensor<2x2xi32>
// CHECK:     return %3, %4 : tensor<2x2xf32>, tensor<2x2xi32>

// -----

// CHECK-LABEL: argmin_bool
func.func @argmin_bool(%arg0: tensor<2xi1>) -> tensor<i32> {
  %0 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<2xi32>
  %1 = mhlo.constant dense<false> : tensor<i1>
  %2 = mhlo.constant dense<0> : tensor<i32>
  %3:2 = mhlo.reduce(%arg0 init: %1), (%0 init: %2) across dimensions = [0] : (tensor<2xi1>, tensor<2xi32>, tensor<i1>, tensor<i32>) -> (tensor<i1>, tensor<i32>)
    reducer(%arg1: tensor<i1>, %arg3: tensor<i1>) (%arg2: tensor<i32>, %arg4: tensor<i32>)  {
    %4 = mhlo.compare  LT, %arg1, %arg3 : (tensor<i1>, tensor<i1>) -> tensor<i1>
    %5 = mhlo.select %4, %arg1, %arg3 : tensor<i1>, tensor<i1>
    %6 = mhlo.compare  EQ, %arg1, %arg3 : (tensor<i1>, tensor<i1>) -> tensor<i1>
    %7 = mhlo.compare  LT, %arg2, %arg4 : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %8 = mhlo.and %6, %7 : tensor<i1>
    %9 = mhlo.or %4, %8 : tensor<i1>
    %10 = mhlo.select %9, %arg2, %arg4 : tensor<i1>, tensor<i32>
    mhlo.return %5, %10 : tensor<i1>, tensor<i32>
  }
  return %3#1 : tensor<i32>
}

// CHECK:     %0 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<2xi32>
// CHECK-DAG: %1 = mhlo.constant dense<false> : tensor<i1>
// CHECK:     %2 = mhlo.constant dense<0> : tensor<i32>
// CHECK:     %cst = arith.constant dense<0> : tensor<1xi32>
// CHECK:     %3 = "tfl.reduce_all"(%arg0, %cst) <{keep_dims = false}> : (tensor<2xi1>, tensor<1xi32>) -> tensor<i1>
// CHECK:     %4 = "tfl.arg_min"(%arg0, %cst) : (tensor<2xi1>, tensor<1xi32>) -> tensor<i32>
// CHECK:     return %4 : tensor<i32>

// -----

// CHECK-LABEL: argmax_with_reshaped_iota
func.func @argmax_with_reshaped_iota(%arg0: tensor<1x32x1xf32>) -> (tensor<1x1xf32>, tensor<1x1xi32>) {
  %0 = mhlo.constant dense<0xFF800000> : tensor<f32>
  %1 = mhlo.constant dense<0> : tensor<i32>
  %2 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<32xi32>
  %3 = "mhlo.reshape"(%2) : (tensor<32xi32>) -> tensor<1x32x1xi32>
  %4:2 = mhlo.reduce(%arg0 init: %0), (%3 init: %1) across dimensions = [1] : (tensor<1x32x1xf32>, tensor<1x32x1xi32>, tensor<f32>, tensor<i32>) -> (tensor<1x1xf32>, tensor<1x1xi32>)
   reducer(%arg1: tensor<f32>, %arg3: tensor<f32>) (%arg2: tensor<i32>, %arg4: tensor<i32>)  {
    %5 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction GT>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %6 = "mhlo.compare"(%arg1, %arg1) {comparison_direction = #mhlo<comparison_direction NE>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %7 = mhlo.or %5, %6 : tensor<i1>
    %8 = "mhlo.select"(%7, %arg1, %arg3) : (tensor<i1>, tensor<f32>, tensor<f32>) -> tensor<f32>
    %9 = "mhlo.compare"(%arg1, %arg3) {comparison_direction = #mhlo<comparison_direction EQ>} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    %10 = "mhlo.compare"(%arg2, %arg4) {comparison_direction = #mhlo<comparison_direction LT>} : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %11 = mhlo.and %9, %10 : tensor<i1>
    %12 = mhlo.or %7, %11 : tensor<i1>
    %13 = "mhlo.select"(%12, %arg2, %arg4) : (tensor<i1>, tensor<i32>, tensor<i32>) -> tensor<i32>
    "mhlo.return"(%8, %13) : (tensor<f32>, tensor<i32>) -> ()
  }
  func.return %4#0, %4#1 : tensor<1x1xf32>, tensor<1x1xi32>
}

// CHECK-DAG: %0 = mhlo.constant dense<0xFF800000> : tensor<f32>
// CHECK:     %1 = mhlo.constant dense<0> : tensor<i32>
// CHECK:     %2 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<32xi32>
// CHECK:     %3 = mhlo.reshape %2 : (tensor<32xi32>) -> tensor<1x32x1xi32>
// CHECK:     %cst = arith.constant dense<1> : tensor<1xi32>
// CHECK:     %4 = "tfl.reduce_max"(%arg0, %cst) <{keep_dims = false}> : (tensor<1x32x1xf32>, tensor<1xi32>) -> tensor<1x1xf32>
// CHECK:     %5 = "tfl.arg_max"(%arg0, %cst) : (tensor<1x32x1xf32>, tensor<1xi32>) -> tensor<1x1xi32>
// CHECK:     return %4, %5 : tensor<1x1xf32>, tensor<1x1xi32>

// -----

// CHECK-LABEL: pytorch_argmax
func.func @pytorch_argmax(%arg0: tensor<1x9xi32>) -> tensor<1xi32> {
  %0 = mhlo.constant dense<0> : tensor<i32>
  %1 = mhlo.constant dense<-2147483648> : tensor<i32>
  %2 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<9xi32>
  %3 = mhlo.reshape %2 : (tensor<9xi32>) -> tensor<1x9xi32>
  %4:2 = mhlo.reduce(%arg0 init: %1), (%3 init: %0) across dimensions = [1] : (tensor<1x9xi32>, tensor<1x9xi32>, tensor<i32>, tensor<i32>) -> (tensor<1xi32>, tensor<1xi32>)
    reducer(%arg1: tensor<i32>, %arg3: tensor<i32>) (%arg2: tensor<i32>, %arg4: tensor<i32>)  {
    %6 = mhlo.compare  GE, %arg1, %arg3 : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %7 = mhlo.select %6, %arg1, %arg3 : tensor<i1>, tensor<i32>
    %8 = mhlo.compare  EQ, %arg1, %arg3 : (tensor<i32>, tensor<i32>) -> tensor<i1>
    %9 = mhlo.minimum %arg2, %arg4 : tensor<i32>
    %10 = mhlo.select %6, %arg2, %arg4 : tensor<i1>, tensor<i32>
    %11 = mhlo.select %8, %9, %10 : tensor<i1>, tensor<i32>
    mhlo.return %7, %11 : tensor<i32>, tensor<i32>
  }
  func.return %4#1 : tensor<1xi32>
}

// CHECK:     %0 = mhlo.constant dense<0> : tensor<i32>
// CHECK-DAG: %1 = mhlo.constant dense<-2147483648> : tensor<i32>
// CHECK:     %2 = "mhlo.iota"() <{iota_dimension = 0 : i64}> : () -> tensor<9xi32>
// CHECK:     %3 = mhlo.reshape %2 : (tensor<9xi32>) -> tensor<1x9xi32>
// CHECK:     %cst = arith.constant dense<1> : tensor<1xi32>
// CHECK:     %4 = "tfl.reduce_max"(%arg0, %cst) <{keep_dims = false}> : (tensor<1x9xi32>, tensor<1xi32>) -> tensor<1xi32>
// CHECK:     %5 = "tfl.arg_max"(%arg0, %cst) : (tensor<1x9xi32>, tensor<1xi32>) -> tensor<1xi32>
// CHECK:     return %5 : tensor<1xi32>

// -----

//===----------------------------------------------------------------------===//
// mhlo.cbrt
//===----------------------------------------------------------------------===//

// CHECK-LABEL: cbrt_f32
func.func @cbrt_f32(%arg0: tensor<1x32x1xf32>) -> tensor<1x32x1xf32> {
  %0 = "mhlo.cbrt"(%arg0) : (tensor<1x32x1xf32>) -> tensor<1x32x1xf32>
  func.return %0 : tensor<1x32x1xf32>
}

// CHECK: %cst = arith.constant dense<1.000000e+00> : tensor<f32>
// CHECK: %cst_0 = arith.constant dense<3.000000e+00> : tensor<f32>
// CHECK: %0 = tfl.div %cst, %cst_0 {fused_activation_function = "NONE"} : tensor<f32>
// CHECK: %1 = tfl.pow(%arg0, %0) : (tensor<1x32x1xf32>, tensor<f32>) -> tensor<1x32x1xf32>
// CHECK: return %1 : tensor<1x32x1xf32>

// -----

// CHECK-LABEL: cbrt_f64
func.func @cbrt_f64(%arg0: tensor<1x32x1xf64>) -> tensor<1x32x1xf64> {
  %0 = "mhlo.cbrt"(%arg0) : (tensor<1x32x1xf64>) -> tensor<1x32x1xf64>
  func.return %0 : tensor<1x32x1xf64>
}

// CHECK-NOT: tfl

// -----

//===----------------------------------------------------------------------===//
// mhlo.convolution
//===----------------------------------------------------------------------===//

// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc
func.func @conv2d_nhwc_ohwi_nhwc(%input: tensor<1x256x256x3xf32>, %filter: tensor<2x1x1x3xf32>) -> tensor<1x256x256x2xf32> {
  %0 = mhlo.convolution(%input, %filter)
    dim_numbers = [b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f],
    window = {stride = [1, 1], pad = [[0, 0], [0, 0]]} {
    batch_group_count = 1 : i64,
    feature_group_count = 1 : i64,
    window_strides = dense<1> : tensor<2xi64>,
    padding = dense<0> : tensor<2x2xi64>,
    rhs_dilation = dense<[1, 1]> : tensor<2xi64>,
    lhs_dilation = dense<[1, 1]> : tensor<2xi64>
  } : (tensor<1x256x256x3xf32>, tensor<2x1x1x3xf32>) -> tensor<1x256x256x2xf32>
  func.return %0 : tensor<1x256x256x2xf32>
}

// CHECK: "tfl.conv_2d"(%arg0, %arg1, %cst) <{dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 1 : i32, stride_w = 1 : i32}> : (tensor<1x256x256x3xf32>, tensor<2x1x1x3xf32>, tensor<2xf32>) -> tensor<1x256x256x2xf32>

// -----

// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc_no_strides
func.func @conv2d_nhwc_ohwi_nhwc_no_strides(%arg0: tensor<1x64x64x4xf32>, %arg1: tensor<320x3x3x4xf32>) -> tensor<1x62x62x320xf32> {
	%0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<[1, 1]> : tensor<2xi64>,
    padding = dense<0> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<[1, 1]> : tensor<2xi64>
  } : (tensor<1x64x64x4xf32>, tensor<320x3x3x4xf32>) -> tensor<1x62x62x320xf32>
  func.return %0 : tensor<1x62x62x320xf32>
}

// CHECK: "tfl.conv_2d"(%arg0, %arg1, %cst) <{dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 1 : i32, stride_w = 1 : i32}> : (tensor<1x64x64x4xf32>, tensor<320x3x3x4xf32>, tensor<320xf32>) -> tensor<1x62x62x320xf32>

// -----

// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc_no_padding
func.func @conv2d_nhwc_ohwi_nhwc_no_padding(%arg0: tensor<1x6x6x207xf32>, %arg1: tensor<16x3x3x207xf32>) -> tensor<1x4x4x16xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<1x6x6x207xf32>, tensor<16x3x3x207xf32>) -> tensor<1x4x4x16xf32>
  func.return %0 : tensor<1x4x4x16xf32>
}

// CHECK: "tfl.conv_2d"(%arg0, %arg1, %cst) <{dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 1 : i32, stride_w = 1 : i32}> : (tensor<1x6x6x207xf32>, tensor<16x3x3x207xf32>, tensor<16xf32>) -> tensor<1x4x4x16xf32>

// -----

// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc_no_lhs_dilation
func.func @conv2d_nhwc_ohwi_nhwc_no_lhs_dilation(%arg0: tensor<1x6x6x207xf32>, %arg1: tensor<16x3x3x207xf32>) -> tensor<1x4x4x16xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    padding = dense<0> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<1x6x6x207xf32>, tensor<16x3x3x207xf32>) -> tensor<1x4x4x16xf32>
  func.return %0 : tensor<1x4x4x16xf32>
}

// CHECK: "tfl.conv_2d"(%arg0, %arg1, %cst) <{dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 1 : i32, stride_w = 1 : i32}> : (tensor<1x6x6x207xf32>, tensor<16x3x3x207xf32>, tensor<16xf32>) -> tensor<1x4x4x16xf32>

// -----

// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc_no_rhs_dilation
func.func @conv2d_nhwc_ohwi_nhwc_no_rhs_dilation(%arg0: tensor<1x6x6x207xf32>, %arg1: tensor<16x3x3x207xf32>) -> tensor<1x4x4x16xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    padding = dense<0> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    lhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<1x6x6x207xf32>, tensor<16x3x3x207xf32>) -> tensor<1x4x4x16xf32>
  func.return %0 : tensor<1x4x4x16xf32>
}

// CHECK: "tfl.conv_2d"(%arg0, %arg1, %cst) <{dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 1 : i32, stride_w = 1 : i32}> : (tensor<1x6x6x207xf32>, tensor<16x3x3x207xf32>, tensor<16xf32>) -> tensor<1x4x4x16xf32>

// -----

// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc_no_strides
func.func @conv2d_nhwc_ohwi_nhwc_no_strides(%arg0: tensor<1x6x6x207xf32>, %arg1: tensor<16x3x3x207xf32>) -> tensor<1x4x4x16xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    padding = dense<0> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    lhs_dilation = dense<1> : tensor<2xi64>,
    rhs_dilation = dense<1> : tensor<2xi64>
  } : (tensor<1x6x6x207xf32>, tensor<16x3x3x207xf32>) -> tensor<1x4x4x16xf32>
  func.return %0 : tensor<1x4x4x16xf32>
}

// CHECK: "tfl.conv_2d"(%arg0, %arg1, %cst) <{dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 1 : i32, stride_w = 1 : i32}> : (tensor<1x6x6x207xf32>, tensor<16x3x3x207xf32>, tensor<16xf32>) -> tensor<1x4x4x16xf32>

// -----

// TODO: b/351437662 - Add support for dynamic batch.
// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc
func.func @conv2d_nhwc_ohwi_nhwc_dynamic_batch(%input: tensor<?x256x256x3xf32>, %filter: tensor<2x1x1x3xf32>) -> tensor<?x256x256x2xf32> {
  %0 = mhlo.convolution(%input, %filter)
    dim_numbers = [b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f],
    window = {stride = [1, 1], pad = [[0, 0], [0, 0]]} {
    batch_group_count = 1 : i64,
    feature_group_count = 1 : i64
  } : (tensor<?x256x256x3xf32>, tensor<2x1x1x3xf32>) -> tensor<?x256x256x2xf32>
  func.return %0 : tensor<?x256x256x2xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
// CHECK-LABEL: conv2d_nhwc_hwio_nhwc
func.func @conv2d_nhwc_hwio_nhwc(%arg0: tensor<1x8x8x207xf32>, %arg1: tensor<3x3x207x16xf32>) -> tensor<1x8x8x16xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    padding = dense<1> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<1x8x8x207xf32>, tensor<3x3x207x16xf32>) -> tensor<1x8x8x16xf32>
  func.return %0 : tensor<1x8x8x16xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
// CHECK-LABEL: conv2d_nhwc_hwio_hwnc
func.func @conv2d_nhwc_hwio_hwnc(%arg0: tensor<1x8x8x207xf32>, %arg1: tensor<3x3x207x16xf32>) -> tensor<8x8x1x16xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[0, 1, i, o]->[0, 1, b, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    padding = dense<1> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<1x8x8x207xf32>, tensor<3x3x207x16xf32>) -> tensor<8x8x1x16xf32>
  func.return %0 : tensor<8x8x1x16xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
// CHECK-LABEL: conv2d_nhwc_hwio_nhwc_dynamic_batch
func.func @conv2d_nhwc_hwio_nhwc_dynamic_batch(%arg0: tensor<?x8x8x207xf32>, %arg1: tensor<3x3x207x16xf32>) -> tensor<?x8x8x16xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    padding = dense<1> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<?x8x8x207xf32>, tensor<3x3x207x16xf32>) -> tensor<?x8x8x16xf32>
  func.return %0 : tensor<?x8x8x16xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc_explicit_padding
func.func @conv2d_nhwc_ohwi_nhwc_explicit_padding(%arg0: tensor<64x8x8x8xf32>, %arg1: tensor<64x8x8x8xf32>) -> tensor<64x3x3x64xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    padding = dense<1> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<64x8x8x8xf32>, tensor<64x8x8x8xf32>) -> tensor<64x3x3x64xf32>
  func.return %0 : tensor<64x3x3x64xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc_explicit_padding_dynamic_batch
func.func @conv2d_nhwc_ohwi_nhwc_explicit_padding_dynamic_batch(%arg0: tensor<?x8x8x8xf32>, %arg1: tensor<64x8x8x8xf32>) -> tensor<?x3x3x64xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    padding = dense<1> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<?x8x8x8xf32>, tensor<64x8x8x8xf32>) -> tensor<?x3x3x64xf32>
  func.return %0 : tensor<?x3x3x64xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc_negative_explicit_padding
func.func @conv2d_nhwc_ohwi_nhwc_negative_explicit_padding(%arg0: tensor<128x7x9x64xf32>, %arg1: tensor<4x3x2x64xf32>) -> tensor<128x4x3x4xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    padding = dense<[[4, -2], [-5, 2]]> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<2> : tensor<2xi64>
  } : (tensor<128x7x9x64xf32>, tensor<4x3x2x64xf32>) -> tensor<128x4x3x4xf32>
  func.return %0 : tensor<128x4x3x4xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
// CHECK-LABEL: conv2d_nhwc_ohwi_nhwc_negative_explicit_padding_dynamic_batch
func.func @conv2d_nhwc_ohwi_nhwc_negative_explicit_padding_dynamic_batch(%arg0: tensor<?x7x9x64xf32>, %arg1: tensor<4x3x2x64xf32>) -> tensor<?x4x3x4xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    padding = dense<[[4, -2], [-5, 2]]> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<2> : tensor<2xi64>
  } : (tensor<?x7x9x64xf32>, tensor<4x3x2x64xf32>) -> tensor<?x4x3x4xf32>
  func.return %0 : tensor<?x4x3x4xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Add support for depthwise conv.
// CHECK-LABEL: depthwise_conv2d_nhwc_ohwi_nhwc
func.func @depthwise_conv2d_nhwc_ohwi_nhwc(%arg0: tensor<1x8x8x207xf32>, %arg1: tensor<3312x3x3x1xf32>) -> tensor<1x8x8x3312xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 207 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    padding = dense<1> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<1x8x8x207xf32>, tensor<3312x3x3x1xf32>) -> tensor<1x8x8x3312xf32>
  func.return %0 : tensor<1x8x8x3312xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Add support for conv to resize.
// CHECK-LABEL: conv2d_resize_perferred_nhwc_hwoi_nhwc
func.func @conv2d_resize_perferred_nhwc_hwoi_nhwc(%arg0: tensor<1x56x1248x16xf32>, %arg1: tensor<16x3x1x1xf32>) -> tensor<1x111x1248x16xf32> {
	%0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 16 : i64,
    lhs_dilation = dense<[2, 1]> : tensor<2xi64>,
    padding = dense<[[1, 1], [0, 0]]> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<[1, 1]> : tensor<2xi64>,
    window_strides = dense<[1, 1]> : tensor<2xi64>
  } : (tensor<1x56x1248x16xf32>, tensor<16x3x1x1xf32>) -> tensor<1x111x1248x16xf32>
  func.return %0 : tensor<1x111x1248x16xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Add support for conv to resize.
// CHECK-LABEL: conv2d_to_resize_nhwc_hwoi_nhwc
func.func @conv2d_to_resize_nhwc_hwoi_nhwc(%arg0: tensor<1x56x624x16xf32>, %arg1: tensor<16x1x257x1xf32>) -> tensor<1x56x904x16xf32> {
	%0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 16 : i64,
    lhs_dilation = dense<[1, 129]> : tensor<2xi64>,
    padding = dense<[[0, 0], [128, 128]]> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<[1, 89]> : tensor<2xi64>
  } : (tensor<1x56x624x16xf32>, tensor<16x1x257x1xf32>) -> tensor<1x56x904x16xf32>
  func.return %0 : tensor<1x56x904x16xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Add support for feature groups.
// CHECK-LABEL: group_conv2d_nhwc_ohwi_nhwc
func.func @group_conv2d_nhwc_ohwi_nhwc(%arg0: tensor<1x14x14x2240xf32>, %arg1: tensor<2240x3x3x112xf32>) -> tensor<1x7x7x2240xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[o, 0, 1, i]->[b, 0, 1, f]>,
    feature_group_count = 20 : i64,
    lhs_dilation = dense<1> : tensor<2xi64>,
    padding = dense<1> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_reversal = dense<false> : tensor<2xi1>,
    window_strides = dense<2> : tensor<2xi64>
  } : (tensor<1x14x14x2240xf32>, tensor<2240x3x3x112xf32>) -> tensor<1x7x7x2240xf32>
  func.return %0 : tensor<1x7x7x2240xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
// CHECK-LABEL: conv2d_back_prop_input_same_pad_nhwc_hwoi_nhwc
func.func @conv2d_back_prop_input_same_pad_nhwc_hwoi_nhwc(%arg0: tensor<1x256x256x2xf32>, %arg1: tensor<4x4x2x2xf32>) -> tensor<1x512x512x2xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[0, 1, o, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<2> : tensor<2xi64>,
    padding = dense<[[2, 2], [2, 2]]> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<1x256x256x2xf32>, tensor<4x4x2x2xf32>) -> tensor<1x512x512x2xf32>
  func.return %0 : tensor<1x512x512x2xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
// CHECK-LABEL: conv2d_back_prop_input_nhwc_hwoi_nhwc
func.func @conv2d_back_prop_input_nhwc_hwoi_nhwc(%arg0: tensor<8x4x4x32xf32>, %arg1: tensor<3x3x64x32xf32>) -> tensor<8x8x8x64xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[0, 1, o, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<2> : tensor<2xi64>,
    padding = dense<[[2, 1], [2, 1]]> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<8x4x4x32xf32>, tensor<3x3x64x32xf32>) -> tensor<8x8x8x64xf32>
  func.return %0 : tensor<8x8x8x64xf32>
}

// CHECK-NOT: tfl

// -----

// TODO: b/351437662 - Copy this test case to "prepare" phase when it exists.
func.func @conv2d_back_prop_input_negative_pad_nhwc_hwoi_nhwc(%arg0: tensor<1x256x256x2xf32>, %arg1: tensor<4x4x2x2xf32>) -> tensor<1x504x504x2xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {
    batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<[b, 0, 1, f]x[0, 1, o, i]->[b, 0, 1, f]>,
    feature_group_count = 1 : i64,
    lhs_dilation = dense<2> : tensor<2xi64>,
    padding = dense<[[-2, -2], [-2, -2]]> : tensor<2x2xi64>,
    precision_config = [#mhlo<precision DEFAULT>, #mhlo<precision DEFAULT>],
    rhs_dilation = dense<1> : tensor<2xi64>,
    window_strides = dense<1> : tensor<2xi64>
  } : (tensor<1x256x256x2xf32>, tensor<4x4x2x2xf32>) -> tensor<1x504x504x2xf32>
  func.return %0 : tensor<1x504x504x2xf32>
}

// CHECK-NOT: tfl

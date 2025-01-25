// RUN: mlir_fusions_opt  %s -split-input-file -verify-diagnostics

#map0 = #xla_gpu.indexing_map<"(d0, d1)[s0] -> (d0, d1 + s0), domain: d0 in [1, 2], d1 in [5, 8], s0 in [0, 32]">
func.func @apply_indexing(%d0: index, %d1: index, %s0: index) -> (index, index) {
  // expected-error @+1 {{operand count must match the number of dimensions and symbols in the affine map}}
  %0:2 = xla_gpu.apply_indexing #map0 (%d0)
  func.return %0#0, %0#1 : index, index
}

// -----

#map0 = #xla_gpu.indexing_map<"(d0, d1)[s0] -> (d0, d1 + s0), domain: d0 in [1, 2], d1 in [5, 8], s0 in [0, 32], d0 mod 2 in [0, 1], d0 + s0 in [1, 10]">
func.func @cannot_have_constraints(%d0: index, %d1: index, %s0: index) -> (index, index) {
  // expected-error @+1 {{apply indexing op cannot have any constraints}}
  %0:2 = xla_gpu.apply_indexing #map0 (%d0, %d1)[%s0]
  func.return %0#0, %0#1 : index, index
}

// -----

#map = #xla_gpu.indexing_map<"()[s0, s1] -> (s0, s1), domain: s0 in [0, 1024], s1 in [0, 32]">
func.func @loop_result_num_mismatch(%input: tensor<1024x32xf32>,
    %init: f32) -> (f32) {
  // expected-error @+1 {{mismatch in number of loop-carried values and results}}
   %sum:2 = "xla_gpu.loop"(%init) <{
      indexing_map_attr = #map,
      operandSegmentSizes = array<i32: 0, 1>
    }> ({
    ^bb0(%i: index, %j: index, %r0: index, %r1: index, %iter: f32):
      %t = tensor.extract %input[%i, %j] : tensor<1024x32xf32>
      %add = arith.addf %iter, %t : f32
      xla_gpu.yield %add : f32
    }) : (f32) -> (f32, f32)
  func.return %sum#0 : f32
}

// -----

#map = #xla_gpu.indexing_map<"()[s0] -> (s0, s0), domain: s0 in [0, 1024]">
func.func @loop_iv_num_mismatch(%input: tensor<1024x32xf32>,
    %init: f32) -> (f32) {
  // expected-error @+1 {{mismatch in number of induction variables 2 and RangeVars}}
   %sum = "xla_gpu.loop"(%init) <{
      indexing_map_attr = #map,
      operandSegmentSizes = array<i32: 0, 1>
    }> ({
    ^bb0(%i: index, %j: index, %r0: index, %r1: index, %iter: f32):
      %t = tensor.extract %input[%i, %j] : tensor<1024x32xf32>
      %add = arith.addf %iter, %t : f32
      xla_gpu.yield %add : f32
    }) : (f32) -> (f32)
  func.return %sum : f32
}

// -----

#map = #xla_gpu.indexing_map<"()[s0, s1] -> (s0, s1), domain: s0 in [0, 1024], s1 in [0, 32]">

func.func @loop_types_mismatch(%input: tensor<1024x32xf32>, %init: f32) -> (i32) {
  // expected-error @+1 {{block iter arg type = 'f32', result type = 'i32' and init operand type = 'f32' should match}}
   %sum = "xla_gpu.loop"(%init) <{
      indexing_map_attr = #map,
      operandSegmentSizes = array<i32: 0, 1>
    }> ({
    ^bb0(%i: index, %j: index, %r0: index, %r1: index, %iter: f32):
      %t = tensor.extract %input[%i, %j] : tensor<1024x32xf32>
      %add = arith.addf %iter, %t : f32
      xla_gpu.yield %add : f32
    }) : (f32) -> (i32)
  func.return %sum : i32
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (s0, s1), domain: d0 in [0, 3], s0 in [0, 1024], s1 in [0, 32]">

func.func @loop_op(%input: tensor<1024x32xf32>, %init: f32, %dim: index) -> (f32) {
  // expected-error @+1 {{mismatch in number of dims operands 0 and DimVars in the indexing map}}
  %sum = xla_gpu.loop ()[%i, %j] -> (%r0, %r1)
     in #map iter_args(%sum_ = %init) -> (f32) {
    %t = tensor.extract %input[%i, %j] : tensor<1024x32xf32>
    %add = arith.addf %sum_, %t : f32
    xla_gpu.yield %add : f32
  } {xla.range = [0 : index, 42 : index]}
  func.return %sum : f32
}

// -----

#map = #xla_gpu.indexing_map<"(d0, d1)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], d1 in [0, 2], s0 in [0, 1024], s1 in [0, 32]">

func.func @indicies_mismatch(%input: tensor<32x64xf32>, %thread_id: index,
    %output: tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map> {
  // expected-error @+1 {{number of indices must match number of dimensions of indexing map}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id)
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map>
}

// -----

#map = #xla_gpu.indexing_map<"()[s0, s1] -> (s0, s1), domain: s0 in [0, 1024], s1 in [0, 32]">
#map1 = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 32]">

func.func @no_thread_id_in(%input: tensor<32x64xf32>,
    %output: tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{must have thread_id dimension in both indexing maps}}
  %0 = xla_gpu.materialize @exp(%input) at #map()
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 32]">
#map1 = #xla_gpu.indexing_map<"()[s0, s1] -> (s0, s1), domain: s0 in [0, 1024], s1 in [0, 32]">

func.func @no_thread_id_out(%input: tensor<32x64xf32>, %thread_id: index,
    %output: tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{must have thread_id dimension in both indexing maps}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id)
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 32]">
#map1 = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 64], s0 in [0, 1024], s1 in [0, 32]">
func.func @thread_id_bounds_mismatch(%input: tensor<32x64xf32>, %thread_id: index, %output: tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{thread_id dimension must have the same bounds in both indexing maps}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id) : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 32], d0 + s0 in [0, 1024]">
#map1 = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 32]">

func.func @thread_id_constraints_mismatch(%input: tensor<32x64xf32>,
    %thread_id: index, %output: tensor<32x64xf32>)
     -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{constraints of indexing maps must be equal for the thread_id dimension}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id)
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0] -> (d0 + s0, s0), domain: d0 in [0, 32], s0 in [0, 1024]">
#map1 = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 32]">
func.func @symbol_count_mismatch(%input: tensor<32x64xf32>, %thread_id: index, %output: tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{number of symbols in both indexing_maps must match}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id) : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64]">
#map1 = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 32]">
func.func @symbol_domain_mismatch(%input: tensor<32x64xf32>, %thread_id: index, %output: tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{domain of symbols of indexing_maps must match}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id) : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64], s0 + s1 in [0, 1024]">
#map1 = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64], s0 + s1 in [0, 32]">
func.func @symbol_constraints_mismatch(%input: tensor<32x64xf32>,
    %thread_id: index, %output: tensor<32x64xf32>)
    -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{constraints of indexing maps must be equal for all symbols}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id)
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64], s0 mod 2 in [0, 0]">
#map1 = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64], s0 + s1 in [0, 32]">

func.func @symbol_constraint_mismatch(%input: tensor<32x64xf32>,
  %thread_id: index, %output: tensor<32x64xf32>)
  -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{constraints of indexing maps must be equal for all symbols}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id) : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64], s0 + s1 in [0, 1024]">
#map1 = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64], s0 + s1 in [0, 32]">

func.func @symbol_constraint_interval_mismatch(%input: tensor<32x64xf32>,
    %thread_id: index, %output: tensor<32x64xf32>)
    -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{constraints of indexing maps must be equal for all symbols}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id)
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64]">
#map1 = #xla_gpu.indexing_map<"(d0, d1)[s0, s1] -> (d0 + s0, d1 + s1), domain: d0 in [0, 32], d1 in [0, 64], s0 in [0, 1024], s1 in [0, 64]">
func.func @vector_mapping_depends_on_block_id(%input: tensor<32x64xf32>,
    %thread_id: index, %output: tensor<32x64xf32>)
    -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{vector mapping indices must not depend on the block ID}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id)
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0, d1)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], d1 in [0, 64], s0 in [0, 1024], s1 in [0, 64], d1 mod 2 in [0, 0]">
#map1 = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64]">

func.func @block_id_constraints_mismatch(%input: tensor<32x64xf32>,
    %thread_id: index, %block_id: index, %output: tensor<32x64xf32>)
    -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{constraints of indexing maps must be equal for the block_id dimension}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id, %block_id)
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], s0 in [0, 1024], s1 in [0, 64]">
#map1 = #xla_gpu.indexing_map<"(d0, d1)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], d1 in [0, 64], s0 in [0, 1024], s1 in [0, 64], d1 mod 2 in [0, 0]">

func.func @block_id_constraints_mismatch(%input: tensor<32x64xf32>,
    %thread_id: index, %block_id: index, %output: tensor<32x64xf32>)
    -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{constraints of indexing maps must be equal for the block_id dimension}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id)
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0, d1)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], d1 in [0, 64], s0 in [0, 1024], s1 in [0, 64], d1 mod 2 in [0, 0]">
#map1 = #xla_gpu.indexing_map<"(d0, d1)[s0, s1] -> (d0 + s0, s1), domain: d0 in [0, 32], d1 in [0, 64], s0 in [0, 1024], s1 in [0, 64], d1 mod 4 in [0, 0]">

func.func @block_id_constraints_mismatch(%input: tensor<32x64xf32>,
    %thread_id: index, %block_id: index, %output: tensor<32x64xf32>)
    -> !xla_gpu.indexed_vector<32x64xf32, #map1> {
  // expected-error @+1 {{constraints of indexing maps must be equal for the block_id dimension}}
  %0 = xla_gpu.materialize @exp(%input) at #map(%thread_id, %block_id)
    : (tensor<32x64xf32>) -> !xla_gpu.indexed_vector<32x64xf32, #map1>
  func.return %0 : !xla_gpu.indexed_vector<32x64xf32, #map1>
}

// -----

#map = #xla_gpu.indexing_map<"(d0, d1)[s0, s1] -> (d1*32+d0*2+s0, s1), domain: d0 in [0, 32], d1 in [0, 8], s0 in [0, 1], s1 in [0, 1]">
#map1 = #xla_gpu.indexing_map<"(d0, d1)[s0] -> (d0 mod 16 + s0, d1), domain: d0 in [0, 32], d1 in [0, 2], s0 in [0, 1]">

func.func @insert(%input: !xla_gpu.indexed_vector<32x64xf32, #map>,
    %i: index, %j: index, %output: tensor<32x64xf32>) -> tensor<32x64xf32> {
  // expected-error @+1 {{insert_op map must not have any symbols}}
  %0 = xla_gpu.insert %input(%i, %j) into %output at #map1
    : !xla_gpu.indexed_vector<32x64xf32, #map> -> tensor<32x64xf32>
  func.return %0 : tensor<32x64xf32>
}

// -----

#map = #xla_gpu.indexing_map<"(d0, d1)[s0, s1] -> (d1*32+d0*2+s0, s1), domain: d0 in [0, 32], d1 in [0, 8], s0 in [0, 1], s1 in [0, 1]">
#map1 = #xla_gpu.indexing_map<"(d0, d1, d2) -> (d0 mod 16, d1, d2), domain: d0 in [0, 32], d1 in [0, 2], d2 in [0, 5]">

func.func @insert(%input: !xla_gpu.indexed_vector<32x64xf32, #map>,
    %i: index, %j: index, %output: tensor<32x64xf32>) -> tensor<32x64xf32> {
  // expected-error @+1 {{source map result count must equal insert_op's map's dimension count}}
  %0 = xla_gpu.insert %input(%i, %j) into %output at #map1
    : !xla_gpu.indexed_vector<32x64xf32, #map> -> tensor<32x64xf32>
  func.return %0 : tensor<32x64xf32>
}

// -----

func.func @reduce_missing_combiner(%in0: tensor<16x8x4xf32>, %init0: f32,
    %in1: tensor<16x8x4xi32>, %init1: i32) -> (tensor<8xf32>, tensor<8xi32>) {
  // expected-error @+1 {{combiner `@add` not found}}
  %sum:2 = xla_gpu.reduce (%in0, %in1) inits(%init0, %init1) dimensions=[0, 2]
    combiner=@add {xla.range = [0 : index, 42 : index]}
    : tensor<16x8x4xf32>, tensor<16x8x4xi32> to tensor<8xf32>, tensor<8xi32>
  func.return %sum#0, %sum#1 : tensor<8xf32>, tensor<8xi32>
}

// -----

func.func @add(%a_acc: f32, %b_acc: f32, %a: f32, %b: f32)
    -> (f32, f32) {
  %0 = arith.addf %a_acc, %a : f32
  %1 = arith.addf %b_acc, %b : f32
  func.return %0, %1 : f32, f32
}
func.func @reduce_wrong_combiner_type(%in0: tensor<16x8x4xf32>, %init0: f32,
    %in1: tensor<16x8x4xi32>, %init1: i32) -> (tensor<8xf32>, tensor<8xi32>) {
  // expected-error @+1 {{combiner `@add expected to have type '(f32, i32, f32, i32) -> (f32, i32)' but got '(f32, f32, f32, f32) -> (f32, f32)'}}
  %sum:2 = xla_gpu.reduce (%in0, %in1) inits(%init0, %init1) dimensions=[0, 2]
    combiner=@add {xla.range = [0 : index, 42 : index]}
    : tensor<16x8x4xf32>, tensor<16x8x4xi32> to tensor<8xf32>, tensor<8xi32>
  func.return %sum#0, %sum#1 : tensor<8xf32>, tensor<8xi32>
}

// -----

func.func @reduce_init_type_mismatch(%in: tensor<16x8x4xf32>, %init: i32)
    -> (tensor<16x4xf32>) {
  // expected-error @+1 {{init type 'i32' does not match inferred type 'f32'}}
  %sum = "xla_gpu.reduce"(%in, %init)
    <{combiner = @add, dimensions = array<i64: 1>}>
    : (tensor<16x8x4xf32>, i32) -> tensor<16x4xf32>
  func.return %sum : tensor<16x4xf32>
}

SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<i32>;

fn quadSwapDiagonal_a665b1() -> vec4<i32> {
  var res : vec4<i32> = quadSwapDiagonal(vec4<i32>(1i));
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = quadSwapDiagonal_a665b1();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = quadSwapDiagonal_a665b1();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/quadSwapDiagonal/a665b1.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<i32>;

fn quadSwapDiagonal_a665b1() -> vec4<i32> {
  var res : vec4<i32> = quadSwapDiagonal(vec4<i32>(1i));
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = quadSwapDiagonal_a665b1();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = quadSwapDiagonal_a665b1();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/quadSwapDiagonal/a665b1.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


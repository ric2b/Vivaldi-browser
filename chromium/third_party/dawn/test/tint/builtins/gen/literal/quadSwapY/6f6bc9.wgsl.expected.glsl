SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : f32;

fn quadSwapY_6f6bc9() -> f32 {
  var res : f32 = quadSwapY(1.0f);
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = quadSwapY_6f6bc9();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = quadSwapY_6f6bc9();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/quadSwapY/6f6bc9.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : f32;

fn quadSwapY_6f6bc9() -> f32 {
  var res : f32 = quadSwapY(1.0f);
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = quadSwapY_6f6bc9();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = quadSwapY_6f6bc9();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/quadSwapY/6f6bc9.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


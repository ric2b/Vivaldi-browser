SKIP: INVALID


enable subgroups;
enable subgroups_f16;
enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<f16>;

fn quadSwapX_d60cec() -> vec2<f16> {
  var arg_0 = vec2<f16>(1.0h);
  var res : vec2<f16> = quadSwapX(arg_0);
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = quadSwapX_d60cec();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = quadSwapX_d60cec();
}

Failed to generate: <dawn>/test/tint/builtins/gen/var/quadSwapX/d60cec.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


enable subgroups;
enable subgroups_f16;
enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<f16>;

fn quadSwapX_d60cec() -> vec2<f16> {
  var arg_0 = vec2<f16>(1.0h);
  var res : vec2<f16> = quadSwapX(arg_0);
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = quadSwapX_d60cec();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = quadSwapX_d60cec();
}

Failed to generate: <dawn>/test/tint/builtins/gen/var/quadSwapX/d60cec.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


SKIP: INVALID


enable subgroups;
enable subgroups_f16;
enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<f16>;

fn subgroupMin_d85be6() -> vec2<f16> {
  var res : vec2<f16> = subgroupMin(vec2<f16>(1.0h));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMin_d85be6();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupMin/d85be6.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


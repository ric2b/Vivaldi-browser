SKIP: INVALID


enable subgroups;
enable subgroups_f16;
enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<f16>;

fn subgroupMax_33e339() -> vec4<f16> {
  var res : vec4<f16> = subgroupMax(vec4<f16>(1.0h));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMax_33e339();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupMax/33e339.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


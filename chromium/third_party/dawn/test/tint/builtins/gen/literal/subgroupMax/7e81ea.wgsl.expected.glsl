SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<f32>;

fn subgroupMax_7e81ea() -> vec3<f32> {
  var res : vec3<f32> = subgroupMax(vec3<f32>(1.0f));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMax_7e81ea();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupMax/7e81ea.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


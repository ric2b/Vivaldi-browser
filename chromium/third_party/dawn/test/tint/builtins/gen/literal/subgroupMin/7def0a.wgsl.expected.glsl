SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : f32;

fn subgroupMin_7def0a() -> f32 {
  var res : f32 = subgroupMin(1.0f);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMin_7def0a();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupMin/7def0a.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


SKIP: INVALID


enable subgroups;
enable subgroups_f16;
enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : f16;

fn subgroupMin_8ffadc() -> f16 {
  var res : f16 = subgroupMin(1.0h);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMin_8ffadc();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupMin/8ffadc.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


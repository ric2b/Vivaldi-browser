SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<u32>;

fn subgroupAnd_ad0cd3() -> vec3<u32> {
  var res : vec3<u32> = subgroupAnd(vec3<u32>(1u));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupAnd_ad0cd3();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupAnd/ad0cd3.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<u32>;

fn subgroupMax_b8fb0e() -> vec2<u32> {
  var res : vec2<u32> = subgroupMax(vec2<u32>(1u));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMax_b8fb0e();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupMax/b8fb0e.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


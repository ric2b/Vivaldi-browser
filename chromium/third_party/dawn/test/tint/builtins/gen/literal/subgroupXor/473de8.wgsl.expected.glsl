SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<i32>;

fn subgroupXor_473de8() -> vec2<i32> {
  var res : vec2<i32> = subgroupXor(vec2<i32>(1i));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupXor_473de8();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupXor/473de8.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<i32>;

fn subgroupMin_0bc13a() -> vec2<i32> {
  var res : vec2<i32> = subgroupMin(vec2<i32>(1i));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMin_0bc13a();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupMin/0bc13a.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


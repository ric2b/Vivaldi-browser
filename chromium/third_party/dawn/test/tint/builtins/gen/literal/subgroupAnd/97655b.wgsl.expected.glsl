SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<i32>;

fn subgroupAnd_97655b() -> vec4<i32> {
  var res : vec4<i32> = subgroupAnd(vec4<i32>(1i));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupAnd_97655b();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/subgroupAnd/97655b.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


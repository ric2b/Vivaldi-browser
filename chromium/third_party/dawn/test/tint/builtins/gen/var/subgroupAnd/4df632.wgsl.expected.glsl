SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : u32;

fn subgroupAnd_4df632() -> u32 {
  var arg_0 = 1u;
  var res : u32 = subgroupAnd(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupAnd_4df632();
}

Failed to generate: <dawn>/test/tint/builtins/gen/var/subgroupAnd/4df632.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


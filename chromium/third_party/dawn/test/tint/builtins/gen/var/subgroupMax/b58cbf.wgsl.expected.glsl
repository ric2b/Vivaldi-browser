SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : u32;

fn subgroupMax_b58cbf() -> u32 {
  var arg_0 = 1u;
  var res : u32 = subgroupMax(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMax_b58cbf();
}

Failed to generate: <dawn>/test/tint/builtins/gen/var/subgroupMax/b58cbf.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


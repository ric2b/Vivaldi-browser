SKIP: INVALID


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : u32;

fn subgroupShuffle_54f328() -> u32 {
  var arg_0 = 1u;
  var arg_1 = 1i;
  var res : u32 = subgroupShuffle(arg_0, arg_1);
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = subgroupShuffle_54f328();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupShuffle_54f328();
}

Failed to generate: <dawn>/test/tint/builtins/gen/var/subgroupShuffle/54f328.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


enable subgroups;

@group(0) @binding(0) var<storage, read_write> prevent_dce : u32;

fn subgroupShuffle_54f328() -> u32 {
  var arg_0 = 1u;
  var arg_1 = 1i;
  var res : u32 = subgroupShuffle(arg_0, arg_1);
  return res;
}

@fragment
fn fragment_main() {
  prevent_dce = subgroupShuffle_54f328();
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupShuffle_54f328();
}

Failed to generate: <dawn>/test/tint/builtins/gen/var/subgroupShuffle/54f328.wgsl:41:8 error: GLSL backend does not support extension 'subgroups'
enable subgroups;
       ^^^^^^^^^


SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<i32>;

fn subgroupExclusiveMul_019660() -> vec4<i32> {
  var arg_0 = vec4<i32>(1i);
  var res : vec4<i32> = subgroupExclusiveMul(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveMul_019660();
}

Failed to generate: error: Unknown builtin method: 0x55a5e9c4d498

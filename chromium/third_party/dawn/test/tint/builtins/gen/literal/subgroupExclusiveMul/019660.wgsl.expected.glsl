SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<i32>;

fn subgroupExclusiveMul_019660() -> vec4<i32> {
  var res : vec4<i32> = subgroupExclusiveMul(vec4<i32>(1i));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveMul_019660();
}

Failed to generate: error: Unknown builtin method: 0x560050cab230

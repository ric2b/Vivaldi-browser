SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<i32>;

fn subgroupExclusiveMul_87f23e() -> vec3<i32> {
  var res : vec3<i32> = subgroupExclusiveMul(vec3<i32>(1i));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveMul_87f23e();
}

Failed to generate: error: Unknown builtin method: 0x55e0b80c2230

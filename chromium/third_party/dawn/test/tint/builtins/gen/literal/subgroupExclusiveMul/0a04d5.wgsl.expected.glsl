SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<f32>;

fn subgroupExclusiveMul_0a04d5() -> vec3<f32> {
  var res : vec3<f32> = subgroupExclusiveMul(vec3<f32>(1.0f));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveMul_0a04d5();
}

Failed to generate: error: Unknown builtin method: 0x55718f403230

SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : f32;

fn subgroupExclusiveMul_98b2e4() -> f32 {
  var arg_0 = 1.0f;
  var res : f32 = subgroupExclusiveMul(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveMul_98b2e4();
}

Failed to generate: error: Unknown builtin method: 0x55e68cc5f1c0

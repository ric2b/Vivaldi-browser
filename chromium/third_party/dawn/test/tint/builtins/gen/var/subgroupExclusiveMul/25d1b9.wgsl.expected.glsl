SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<f32>;

fn subgroupExclusiveMul_25d1b9() -> vec2<f32> {
  var arg_0 = vec2<f32>(1.0f);
  var res : vec2<f32> = subgroupExclusiveMul(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveMul_25d1b9();
}

Failed to generate: error: Unknown builtin method: 0x558e0d30e498

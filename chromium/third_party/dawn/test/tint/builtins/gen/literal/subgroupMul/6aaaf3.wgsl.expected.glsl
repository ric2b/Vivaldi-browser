SKIP: INVALID


enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<f16>;

fn subgroupMul_6aaaf3() -> vec2<f16> {
  var res : vec2<f16> = subgroupMul(vec2<f16>(1.0h));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMul_6aaaf3();
}

Failed to generate: error: Unknown builtin method: 0x563692c08230

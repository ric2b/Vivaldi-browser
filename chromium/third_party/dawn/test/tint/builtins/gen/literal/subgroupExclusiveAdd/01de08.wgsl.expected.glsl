SKIP: INVALID


enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<f16>;

fn subgroupExclusiveAdd_01de08() -> vec2<f16> {
  var res : vec2<f16> = subgroupExclusiveAdd(vec2<f16>(1.0h));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveAdd_01de08();
}

Failed to generate: error: Unknown builtin method: 0x5616d463a230

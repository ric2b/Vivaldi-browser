SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<u32>;

fn subgroupMul_dd1333() -> vec4<u32> {
  var res : vec4<u32> = subgroupMul(vec4<u32>(1u));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMul_dd1333();
}

Failed to generate: error: Unknown builtin method: 0x559e4ef60230

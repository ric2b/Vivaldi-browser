SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<u32>;

fn subgroupMul_dc672a() -> vec2<u32> {
  var arg_0 = vec2<u32>(1u);
  var res : vec2<u32> = subgroupMul(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMul_dc672a();
}

Failed to generate: error: Unknown builtin method: 0x55c7f9845498

SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<u32>;

fn subgroupMul_fa781b() -> vec3<u32> {
  var arg_0 = vec3<u32>(1u);
  var res : vec3<u32> = subgroupMul(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupMul_fa781b();
}

Failed to generate: error: Unknown builtin method: 0x562286129498

SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : vec2<u32>;

fn subgroupExclusiveMul_d1d490() -> vec2<u32> {
  var res : vec2<u32> = subgroupExclusiveMul(vec2<u32>(1u));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupExclusiveMul_d1d490();
}

Failed to generate: error: Unknown builtin method: 0x559274ec6230

SKIP: INVALID


@group(0) @binding(0) var<storage, read_write> prevent_dce : u32;

fn subgroupAdd_b61df7() -> u32 {
  var arg_0 = 1u;
  var res : u32 = subgroupAdd(arg_0);
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupAdd_b61df7();
}

Failed to generate: error: Unknown builtin method: 0x55af5f0241c0

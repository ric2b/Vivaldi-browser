SKIP: INVALID


enable f16;

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec3<f16>;

fn subgroupAdd_0dd12a() -> vec3<f16> {
  var res : vec3<f16> = subgroupAdd(vec3<f16>(1.0h));
  return res;
}

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupAdd_0dd12a();
}

Failed to generate: error: Unknown builtin method: 0x5560e98c4230

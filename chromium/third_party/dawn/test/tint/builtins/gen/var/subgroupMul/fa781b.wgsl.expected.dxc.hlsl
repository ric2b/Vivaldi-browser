RWByteAddressBuffer prevent_dce : register(u0);

uint3 subgroupMul_fa781b() {
  uint3 arg_0 = (1u).xxx;
  uint3 res = WaveActiveProduct(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store3(0u, asuint(subgroupMul_fa781b()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store3(0u, asuint(subgroupMul_fa781b()));
  return;
}

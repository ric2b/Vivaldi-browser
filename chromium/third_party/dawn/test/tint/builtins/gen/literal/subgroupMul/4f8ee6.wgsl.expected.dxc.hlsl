RWByteAddressBuffer prevent_dce : register(u0);

uint subgroupMul_4f8ee6() {
  uint res = WaveActiveProduct(1u);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(subgroupMul_4f8ee6()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(subgroupMul_4f8ee6()));
  return;
}

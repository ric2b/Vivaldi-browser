
RWByteAddressBuffer prevent_dce : register(u0);
uint4 subgroupInclusiveMul_1cdf5c() {
  uint4 arg_0 = (1u).xxxx;
  uint4 v = arg_0;
  uint4 res = (WavePrefixProduct(v) * v);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, subgroupInclusiveMul_1cdf5c());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, subgroupInclusiveMul_1cdf5c());
}


RWByteAddressBuffer prevent_dce : register(u0);

vector<float16_t, 2> subgroupExclusiveMul_e88d1c() {
  vector<float16_t, 2> res = WavePrefixProduct((float16_t(1.0h)).xx);
  return res;
}

void fragment_main() {
  prevent_dce.Store<vector<float16_t, 2> >(0u, subgroupExclusiveMul_e88d1c());
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store<vector<float16_t, 2> >(0u, subgroupExclusiveMul_e88d1c());
  return;
}

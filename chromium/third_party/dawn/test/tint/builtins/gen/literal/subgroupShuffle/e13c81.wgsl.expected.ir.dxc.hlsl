
RWByteAddressBuffer prevent_dce : register(u0);
uint4 subgroupShuffle_e13c81() {
  uint4 res = WaveReadLaneAt((1u).xxxx, 1);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, subgroupShuffle_e13c81());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, subgroupShuffle_e13c81());
}


RWByteAddressBuffer prevent_dce : register(u0);

uint4 subgroupShuffleDown_d46304() {
  uint4 res = WaveReadLaneAt((1u).xxxx, (WaveGetLaneIndex() + 1u));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(subgroupShuffleDown_d46304()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(subgroupShuffleDown_d46304()));
  return;
}

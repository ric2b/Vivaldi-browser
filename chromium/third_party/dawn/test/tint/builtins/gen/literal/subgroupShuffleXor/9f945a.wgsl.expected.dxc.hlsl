RWByteAddressBuffer prevent_dce : register(u0);

uint3 subgroupShuffleXor_9f945a() {
  uint3 res = WaveReadLaneAt((1u).xxx, (WaveGetLaneIndex() ^ 1u));
  return res;
}

void fragment_main() {
  prevent_dce.Store3(0u, asuint(subgroupShuffleXor_9f945a()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store3(0u, asuint(subgroupShuffleXor_9f945a()));
  return;
}


RWByteAddressBuffer prevent_dce : register(u0);
int3 subgroupShuffleXor_7435fe() {
  int3 res = WaveReadLaneAt((1).xxx, (WaveGetLaneIndex() ^ 1u));
  return res;
}

void fragment_main() {
  prevent_dce.Store3(0u, asuint(subgroupShuffleXor_7435fe()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store3(0u, asuint(subgroupShuffleXor_7435fe()));
}



RWByteAddressBuffer prevent_dce : register(u0);
int2 subgroupShuffleDown_b41899() {
  int2 res = WaveReadLaneAt((int(1)).xx, (WaveGetLaneIndex() + 1u));
  return res;
}

void fragment_main() {
  prevent_dce.Store2(0u, asuint(subgroupShuffleDown_b41899()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store2(0u, asuint(subgroupShuffleDown_b41899()));
}


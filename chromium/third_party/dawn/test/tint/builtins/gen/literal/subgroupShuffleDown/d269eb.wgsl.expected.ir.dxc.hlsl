
RWByteAddressBuffer prevent_dce : register(u0);
int subgroupShuffleDown_d269eb() {
  int res = WaveReadLaneAt(1, (WaveGetLaneIndex() + 1u));
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(subgroupShuffleDown_d269eb()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(subgroupShuffleDown_d269eb()));
}



RWByteAddressBuffer prevent_dce : register(u0);
float4 subgroupShuffleUp_33d495() {
  float4 res = WaveReadLaneAt((1.0f).xxxx, (WaveGetLaneIndex() - 1u));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(subgroupShuffleUp_33d495()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(subgroupShuffleUp_33d495()));
}



RWByteAddressBuffer prevent_dce : register(u0);
int4 subgroupShuffleXor_bdddba() {
  int4 arg_0 = (int(1)).xxxx;
  uint arg_1 = 1u;
  int4 v = arg_0;
  uint v_1 = arg_1;
  int4 res = WaveReadLaneAt(v, (WaveGetLaneIndex() ^ v_1));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(subgroupShuffleXor_bdddba()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(subgroupShuffleXor_bdddba()));
}


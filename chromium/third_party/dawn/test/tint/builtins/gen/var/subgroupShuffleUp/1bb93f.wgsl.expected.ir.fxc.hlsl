SKIP: INVALID


RWByteAddressBuffer prevent_dce : register(u0);
int subgroupShuffleUp_1bb93f() {
  int arg_0 = 1;
  uint arg_1 = 1u;
  int v = arg_0;
  uint v_1 = arg_1;
  int res = WaveReadLaneAt(v, (WaveGetLaneIndex() - v_1));
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(subgroupShuffleUp_1bb93f()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(subgroupShuffleUp_1bb93f()));
}

FXC validation failure:
<scrubbed_path>(8,32-49): error X3004: undeclared identifier 'WaveGetLaneIndex'


tint executable returned error: exit status 1

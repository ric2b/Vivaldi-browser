RWByteAddressBuffer prevent_dce : register(u0);

uint subgroupBroadcast_c36fe1() {
  uint arg_0 = 1u;
  uint res = WaveReadLaneAt(arg_0, 1u);
  return res;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(subgroupBroadcast_c36fe1()));
  return;
}

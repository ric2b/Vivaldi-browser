RWByteAddressBuffer prevent_dce : register(u0);

int4 quadBroadcast_bed00b() {
  int4 arg_0 = (1).xxxx;
  int4 res = QuadReadLaneAt(arg_0, 1u);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(quadBroadcast_bed00b()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(quadBroadcast_bed00b()));
  return;
}

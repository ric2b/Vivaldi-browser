RWByteAddressBuffer prevent_dce : register(u0);

int4 subgroupAnd_97655b() {
  int4 tint_tmp = (1).xxxx;
  int4 res = asint(WaveActiveBitAnd(asuint(tint_tmp)));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(subgroupAnd_97655b()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(subgroupAnd_97655b()));
  return;
}

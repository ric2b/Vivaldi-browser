
RWByteAddressBuffer prevent_dce : register(u0);
int subgroupAny_cddda0() {
  bool res = WaveActiveAnyTrue(true);
  return ((all((res == false))) ? (1) : (0));
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(subgroupAny_cddda0()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(subgroupAny_cddda0()));
}


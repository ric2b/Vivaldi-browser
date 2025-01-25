RWByteAddressBuffer prevent_dce : register(u0);

RWByteAddressBuffer sb_rw : register(u1);

int sb_rwatomicSub(uint offset, int value) {
  int original_value = 0;
  sb_rw.InterlockedAdd(offset, -value, original_value);
  return original_value;
}


int atomicSub_051100() {
  int res = sb_rwatomicSub(0u, 1);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(atomicSub_051100()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(atomicSub_051100()));
  return;
}

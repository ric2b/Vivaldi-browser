RWByteAddressBuffer prevent_dce : register(u0);
RWByteAddressBuffer sb_rw : register(u1);

uint arrayLength_eb510f() {
  uint tint_symbol_1 = 0u;
  sb_rw.GetDimensions(tint_symbol_1);
  uint tint_symbol_2 = ((tint_symbol_1 - 0u) / 4u);
  uint res = tint_symbol_2;
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(arrayLength_eb510f()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(arrayLength_eb510f()));
  return;
}

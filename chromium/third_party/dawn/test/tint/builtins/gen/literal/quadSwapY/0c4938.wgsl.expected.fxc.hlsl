SKIP: FAILED

RWByteAddressBuffer prevent_dce : register(u0);

uint quadSwapY_0c4938() {
  uint res = QuadReadAcrossY(1u);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(quadSwapY_0c4938()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(quadSwapY_0c4938()));
  return;
}
FXC validation failure:
<scrubbed_path>(4,14-32): error X3004: undeclared identifier 'QuadReadAcrossY'


tint executable returned error: exit status 1

SKIP: FAILED

RWByteAddressBuffer prevent_dce : register(u0);

int4 quadSwapX_edfa1f() {
  int4 res = QuadReadAcrossX((1).xxxx);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(quadSwapX_edfa1f()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(quadSwapX_edfa1f()));
  return;
}
FXC validation failure:
<scrubbed_path>(4,14-38): error X3004: undeclared identifier 'QuadReadAcrossX'


tint executable returned error: exit status 1

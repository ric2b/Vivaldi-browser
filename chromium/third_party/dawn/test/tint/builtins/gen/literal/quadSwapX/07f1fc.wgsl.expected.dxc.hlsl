RWByteAddressBuffer prevent_dce : register(u0);

uint4 quadSwapX_07f1fc() {
  uint4 res = QuadReadAcrossX((1u).xxxx);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(quadSwapX_07f1fc()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(quadSwapX_07f1fc()));
  return;
}

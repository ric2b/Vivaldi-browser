SKIP: FAILED

RWByteAddressBuffer prevent_dce : register(u0);

uint4 quadSwapDiagonal_c31636() {
  uint4 res = QuadReadAcrossDiagonal((1u).xxxx);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(quadSwapDiagonal_c31636()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(quadSwapDiagonal_c31636()));
  return;
}
FXC validation failure:
<scrubbed_path>(4,15-47): error X3004: undeclared identifier 'QuadReadAcrossDiagonal'


tint executable returned error: exit status 1


RWByteAddressBuffer prevent_dce : register(u0);
int quadSwapDiagonal_9ccb38() {
  int arg_0 = 1;
  int res = QuadReadAcrossDiagonal(arg_0);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(quadSwapDiagonal_9ccb38()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(quadSwapDiagonal_9ccb38()));
}


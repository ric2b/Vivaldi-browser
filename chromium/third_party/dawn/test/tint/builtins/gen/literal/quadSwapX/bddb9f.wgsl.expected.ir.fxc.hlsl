SKIP: FAILED


RWByteAddressBuffer prevent_dce : register(u0);
uint3 quadSwapX_bddb9f() {
  uint3 res = QuadReadAcrossX((1u).xxx);
  return res;
}

void fragment_main() {
  prevent_dce.Store3(0u, quadSwapX_bddb9f());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store3(0u, quadSwapX_bddb9f());
}

FXC validation failure:
<scrubbed_path>(4,15-39): error X3004: undeclared identifier 'QuadReadAcrossX'


tint executable returned error: exit status 1

SKIP: INVALID

RWByteAddressBuffer prevent_dce : register(u0);

int subgroupMul_3fe886() {
  int res = WaveActiveProduct(1);
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(subgroupMul_3fe886()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(subgroupMul_3fe886()));
  return;
}
FXC validation failure:
<scrubbed_path>(4,13-32): error X3004: undeclared identifier 'WaveActiveProduct'


tint executable returned error: exit status 1


RWByteAddressBuffer prevent_dce : register(u0);
RWTexture1D<uint4> arg_0 : register(u0, space1);
uint4 textureLoad_17095b() {
  uint arg_1 = 1u;
  RWTexture1D<uint4> v = arg_0;
  uint4 res = uint4(v.Load(int2(int(arg_1), 0)));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, textureLoad_17095b());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, textureLoad_17095b());
}


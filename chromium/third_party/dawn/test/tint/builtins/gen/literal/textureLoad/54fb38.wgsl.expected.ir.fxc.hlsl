
RWByteAddressBuffer prevent_dce : register(u0);
RWTexture2DArray<uint4> arg_0 : register(u0, space1);
uint4 textureLoad_54fb38() {
  RWTexture2DArray<uint4> v = arg_0;
  int2 v_1 = int2((1u).xx);
  uint4 res = uint4(v.Load(int4(v_1, int(1u), 0)));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, textureLoad_54fb38());
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, textureLoad_54fb38());
}


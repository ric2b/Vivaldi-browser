RWByteAddressBuffer prevent_dce : register(u0);
RWTexture2DArray<int4> arg_0 : register(u0, space1);

int4 textureLoad_c5c86d() {
  int4 res = arg_0.Load(int3((1).xx, 1));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_c5c86d()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_c5c86d()));
  return;
}

RWByteAddressBuffer prevent_dce : register(u0);
RWTexture2DArray<float4> arg_0 : register(u0, space1);

float4 textureLoad_2dbfc2() {
  uint2 arg_1 = (1u).xx;
  int arg_2 = 1;
  float4 res = arg_0.Load(uint3(arg_1, uint(arg_2)));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_2dbfc2()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_2dbfc2()));
  return;
}

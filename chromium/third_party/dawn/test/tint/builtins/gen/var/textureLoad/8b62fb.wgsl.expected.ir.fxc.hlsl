
RWByteAddressBuffer prevent_dce : register(u0);
RWTexture2D<float4> arg_0 : register(u0, space1);
float4 textureLoad_8b62fb() {
  uint2 arg_1 = (1u).xx;
  RWTexture2D<float4> v = arg_0;
  float4 res = float4(v.Load(int3(int2(arg_1), 0)));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_8b62fb()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_8b62fb()));
}


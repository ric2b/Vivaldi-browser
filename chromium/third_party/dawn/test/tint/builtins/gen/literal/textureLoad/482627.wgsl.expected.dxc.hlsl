RWByteAddressBuffer prevent_dce : register(u0);
RWTexture2DArray<float4> arg_0 : register(u0, space1);

float4 textureLoad_482627() {
  float4 res = arg_0.Load(uint3((1u).xx, 1u));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_482627()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_482627()));
  return;
}

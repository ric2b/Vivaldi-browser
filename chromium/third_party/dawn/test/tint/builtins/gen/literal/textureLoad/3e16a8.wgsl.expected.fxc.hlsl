RWByteAddressBuffer prevent_dce : register(u0);
RWTexture2D<float4> arg_0 : register(u0, space1);

float4 textureLoad_3e16a8() {
  float4 res = arg_0.Load(int2((1).xx));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_3e16a8()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_3e16a8()));
  return;
}

RWByteAddressBuffer prevent_dce : register(u0);
RWTexture2D<float4> arg_0 : register(u0, space1);

float4 textureLoad_a5e0a5() {
  float4 res = arg_0.Load((1u).xx);
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_a5e0a5()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_a5e0a5()));
  return;
}

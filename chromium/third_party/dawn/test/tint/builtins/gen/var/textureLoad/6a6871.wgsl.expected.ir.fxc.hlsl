
RWByteAddressBuffer prevent_dce : register(u0);
RWTexture3D<float4> arg_0 : register(u0, space1);
float4 textureLoad_6a6871() {
  int3 arg_1 = (1).xxx;
  RWTexture3D<float4> v = arg_0;
  float4 res = float4(v.Load(int4(int3(arg_1), 0)));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_6a6871()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_6a6871()));
}


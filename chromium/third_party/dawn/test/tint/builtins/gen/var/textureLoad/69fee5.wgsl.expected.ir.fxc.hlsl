
RWByteAddressBuffer prevent_dce : register(u0);
RWTexture2DArray<float4> arg_0 : register(u0, space1);
float4 textureLoad_69fee5() {
  uint2 arg_1 = (1u).xx;
  int arg_2 = 1;
  RWTexture2DArray<float4> v = arg_0;
  int v_1 = arg_2;
  int2 v_2 = int2(arg_1);
  float4 res = float4(v.Load(int4(v_2, int(v_1), 0)));
  return res;
}

void fragment_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_69fee5()));
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store4(0u, asuint(textureLoad_69fee5()));
}


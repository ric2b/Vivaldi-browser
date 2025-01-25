RWByteAddressBuffer prevent_dce : register(u0);
RWTexture2DArray<uint4> arg_0 : register(u0, space1);

uint2 textureDimensions_36eeb7() {
  uint3 tint_tmp;
  arg_0.GetDimensions(tint_tmp.x, tint_tmp.y, tint_tmp.z);
  uint2 res = tint_tmp.xy;
  return res;
}

void fragment_main() {
  prevent_dce.Store2(0u, asuint(textureDimensions_36eeb7()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store2(0u, asuint(textureDimensions_36eeb7()));
  return;
}

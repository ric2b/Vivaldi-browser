RWByteAddressBuffer prevent_dce : register(u0);
RWTexture1D<uint4> arg_0 : register(u0, space1);

uint textureDimensions_9944d5() {
  uint tint_tmp;
  arg_0.GetDimensions(tint_tmp);
  uint res = tint_tmp;
  return res;
}

void fragment_main() {
  prevent_dce.Store(0u, asuint(textureDimensions_9944d5()));
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  prevent_dce.Store(0u, asuint(textureDimensions_9944d5()));
  return;
}

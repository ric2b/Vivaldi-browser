RWTexture1D<float4> arg_0 : register(u0, space1);

void textureStore_706236() {
  arg_0[1] = (1.0f).xxxx;
}

void fragment_main() {
  textureStore_706236();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_706236();
  return;
}

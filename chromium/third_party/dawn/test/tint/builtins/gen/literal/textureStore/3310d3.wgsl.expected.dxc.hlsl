RWTexture2DArray<float4> arg_0 : register(u0, space1);

void textureStore_3310d3() {
  arg_0[int3((1).xx, 1)] = (1.0f).xxxx;
}

void fragment_main() {
  textureStore_3310d3();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_3310d3();
  return;
}

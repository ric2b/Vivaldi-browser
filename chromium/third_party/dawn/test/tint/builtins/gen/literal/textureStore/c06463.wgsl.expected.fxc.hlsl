RWTexture2DArray<float4> arg_0 : register(u0, space1);

void textureStore_c06463() {
  arg_0[int3((1).xx, 1)] = (1.0f).xxxx;
}

void fragment_main() {
  textureStore_c06463();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_c06463();
  return;
}

RWTexture2DArray<float4> arg_0 : register(u0, space1);

void textureStore_319029() {
  int2 arg_1 = (1).xx;
  int arg_2 = 1;
  float4 arg_3 = (1.0f).xxxx;
  arg_0[int3(arg_1, arg_2)] = arg_3;
}

void fragment_main() {
  textureStore_319029();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_319029();
  return;
}

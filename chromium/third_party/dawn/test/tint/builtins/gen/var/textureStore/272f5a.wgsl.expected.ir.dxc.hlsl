
RWTexture2DArray<float4> arg_0 : register(u0, space1);
void textureStore_272f5a() {
  int2 arg_1 = (1).xx;
  uint arg_2 = 1u;
  float4 arg_3 = (1.0f).xxxx;
  RWTexture2DArray<float4> v = arg_0;
  int2 v_1 = arg_1;
  float4 v_2 = arg_3;
  v[int3(v_1, int(arg_2))] = v_2;
}

void fragment_main() {
  textureStore_272f5a();
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_272f5a();
}


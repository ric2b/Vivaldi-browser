
RWTexture3D<float4> arg_0 : register(u0, space1);
void textureStore_ed6198() {
  arg_0[(1).xxx] = (1.0f).xxxx;
}

void fragment_main() {
  textureStore_ed6198();
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_ed6198();
}



RWTexture3D<int4> arg_0 : register(u0, space1);
void textureStore_cb3b0b() {
  arg_0[(1u).xxx] = (1).xxxx;
}

void fragment_main() {
  textureStore_cb3b0b();
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_cb3b0b();
}


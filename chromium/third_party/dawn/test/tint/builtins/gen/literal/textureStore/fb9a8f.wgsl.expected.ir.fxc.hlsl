
RWTexture1D<uint4> arg_0 : register(u0, space1);
void textureStore_fb9a8f() {
  arg_0[1] = (1u).xxxx;
}

void fragment_main() {
  textureStore_fb9a8f();
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_fb9a8f();
}


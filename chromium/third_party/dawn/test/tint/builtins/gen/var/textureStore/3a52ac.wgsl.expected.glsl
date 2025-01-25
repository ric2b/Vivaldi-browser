#version 310 es
precision highp float;
precision highp int;

layout(binding = 0, rgba16i) uniform highp writeonly iimage2DArray arg_0;
void textureStore_3a52ac() {
  ivec2 arg_1 = ivec2(1);
  int arg_2 = 1;
  ivec4 arg_3 = ivec4(1);
  imageStore(arg_0, ivec3(arg_1, arg_2), arg_3);
}

void fragment_main() {
  textureStore_3a52ac();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

layout(binding = 0, rgba16i) uniform highp writeonly iimage2DArray arg_0;
void textureStore_3a52ac() {
  ivec2 arg_1 = ivec2(1);
  int arg_2 = 1;
  ivec4 arg_3 = ivec4(1);
  imageStore(arg_0, ivec3(arg_1, arg_2), arg_3);
}

void compute_main() {
  textureStore_3a52ac();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}

#version 460
precision highp float;
precision highp int;

layout(binding = 0, r8) uniform highp image2DArray arg_0;
void textureStore_43d1df() {
  imageStore(arg_0, ivec3(ivec2(1), int(1)), vec4(1.0f));
}
void main() {
  textureStore_43d1df();
}
#version 460

layout(binding = 0, r8) uniform highp image2DArray arg_0;
void textureStore_43d1df() {
  imageStore(arg_0, ivec3(ivec2(1), int(1)), vec4(1.0f));
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  textureStore_43d1df();
}

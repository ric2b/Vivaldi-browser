#version 310 es


struct Results {
  float colorSamples[4];
};

layout(binding = 2, std430)
buffer results_block_1_ssbo {
  Results inner;
} v;
uniform highp sampler2DMS texture0;
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  ivec2 v_1 = ivec2(ivec2(0));
  v.inner.colorSamples[0] = texelFetch(texture0, v_1, int(0))[0u];
}

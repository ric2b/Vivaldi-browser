#version 310 es
#extension GL_AMD_gpu_shader_half_float: require


struct mat4x2_f16_std140 {
  f16vec2 col0;
  f16vec2 col1;
  f16vec2 col2;
  f16vec2 col3;
};

layout(binding = 0, std140)
uniform tint_symbol_1_std140_1_ubo {
  mat4x2_f16_std140 tint_symbol[4];
} v_1;
layout(binding = 1, std430)
buffer tint_symbol_3_1_ssbo {
  float16_t tint_symbol_2;
} v_2;
float16_t a(f16mat4x2 a_1[4]) {
  return a_1[0][0][0u];
}
float16_t b(f16mat4x2 m) {
  return m[0][0u];
}
float16_t c(f16vec2 v) {
  return v[0u];
}
float16_t d(float16_t f) {
  return f;
}
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  mat4x2_f16_std140 v_3[4] = v_1.tint_symbol;
  f16mat4x2 v_4[4] = f16mat4x2[4](f16mat4x2(f16vec2(0.0hf), f16vec2(0.0hf), f16vec2(0.0hf), f16vec2(0.0hf)), f16mat4x2(f16vec2(0.0hf), f16vec2(0.0hf), f16vec2(0.0hf), f16vec2(0.0hf)), f16mat4x2(f16vec2(0.0hf), f16vec2(0.0hf), f16vec2(0.0hf), f16vec2(0.0hf)), f16mat4x2(f16vec2(0.0hf), f16vec2(0.0hf), f16vec2(0.0hf), f16vec2(0.0hf)));
  {
    uint v_5 = 0u;
    v_5 = 0u;
    while(true) {
      uint v_6 = v_5;
      if ((v_6 >= 4u)) {
        break;
      }
      v_4[v_6] = f16mat4x2(v_3[v_6].col0, v_3[v_6].col1, v_3[v_6].col2, v_3[v_6].col3);
      {
        v_5 = (v_6 + 1u);
      }
      continue;
    }
  }
  float16_t v_7 = a(v_4);
  float16_t v_8 = (v_7 + b(f16mat4x2(v_1.tint_symbol[1].col0, v_1.tint_symbol[1].col1, v_1.tint_symbol[1].col2, v_1.tint_symbol[1].col3)));
  float16_t v_9 = (v_8 + c(v_1.tint_symbol[1].col0.yx));
  v_2.tint_symbol_2 = (v_9 + d(v_1.tint_symbol[1].col0.yx[0u]));
}

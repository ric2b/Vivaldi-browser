SKIP: FAILED

struct main_out {
  float4 x_GLF_color_1;
};

struct main_outputs {
  float4 main_out_x_GLF_color_1 : SV_Target0;
};


static float4 x_GLF_color = (0.0f).xxxx;
cbuffer cbuffer_x_6 : register(b0) {
  uint4 x_6[1];
};
void main_1() {
  int GLF_dead6index = 0;
  int GLF_dead6currentNode = 0;
  int donor_replacementGLF_dead6tree[1] = (int[1])0;
  x_GLF_color = float4(1.0f, 0.0f, 0.0f, 1.0f);
  GLF_dead6index = 0;
  if ((asfloat(x_6[0u].y) < 0.0f)) {
    {
      while(true) {
        if (true) {
        } else {
          break;
        }
        GLF_dead6currentNode = donor_replacementGLF_dead6tree[GLF_dead6index];
        GLF_dead6index = GLF_dead6currentNode;
        {
        }
        continue;
      }
    }
  }
}

main_out main_inner() {
  main_1();
  main_out v = {x_GLF_color};
  return v;
}

main_outputs main() {
  main_out v_1 = main_inner();
  main_outputs v_2 = {v_1.x_GLF_color_1};
  return v_2;
}

DXC validation failure:
error: validation errors
hlsl.hlsl:43: error: Loop must have break.
Validation failed.



tint executable returned error: exit status 1

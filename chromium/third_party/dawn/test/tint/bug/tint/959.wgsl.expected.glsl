#version 310 es
precision highp float;
precision highp int;


struct S {
  float a;
};

layout(binding = 0, std430)
buffer b0_block_1_ssbo {
  S inner;
} v;
layout(binding = 0, std430)
buffer b1_block_1_ssbo {
  S inner;
} v_1;
layout(binding = 0, std430)
buffer b2_block_1_ssbo {
  S inner;
} v_2;
layout(binding = 0, std430)
buffer b3_block_1_ssbo {
  S inner;
} v_3;
layout(binding = 0, std430)
buffer b4_block_1_ssbo {
  S inner;
} v_4;
layout(binding = 0, std430)
buffer b5_block_1_ssbo {
  S inner;
} v_5;
layout(binding = 0, std430)
buffer b6_block_1_ssbo {
  S inner;
} v_6;
layout(binding = 0, std430)
buffer b7_block_1_ssbo {
  S inner;
} v_7;
layout(binding = 1, std140)
uniform b8_block_1_ubo {
  S inner;
} v_8;
layout(binding = 1, std140)
uniform b9_block_1_ubo {
  S inner;
} v_9;
layout(binding = 1, std140)
uniform b10_block_1_ubo {
  S inner;
} v_10;
layout(binding = 1, std140)
uniform b11_block_1_ubo {
  S inner;
} v_11;
layout(binding = 1, std140)
uniform b12_block_1_ubo {
  S inner;
} v_12;
layout(binding = 1, std140)
uniform b13_block_1_ubo {
  S inner;
} v_13;
layout(binding = 1, std140)
uniform b14_block_1_ubo {
  S inner;
} v_14;
layout(binding = 1, std140)
uniform b15_block_1_ubo {
  S inner;
} v_15;
uniform highp sampler2D t0;
uniform highp sampler2D t1;
uniform highp sampler2D t2;
uniform highp sampler2D t3;
uniform highp sampler2D t4;
uniform highp sampler2D t5;
uniform highp sampler2D t6;
uniform highp sampler2D t7;
uniform highp sampler2DShadow t8;
uniform highp sampler2DShadow t9;
uniform highp sampler2DShadow t10;
uniform highp sampler2DShadow t11;
uniform highp sampler2DShadow t12;
uniform highp sampler2DShadow t13;
uniform highp sampler2DShadow t14;
uniform highp sampler2DShadow t15;
void main() {
}

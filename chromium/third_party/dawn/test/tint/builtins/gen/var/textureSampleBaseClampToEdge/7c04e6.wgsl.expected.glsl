#version 310 es
precision highp float;
precision highp int;

vec3 tint_select(vec3 param_0, vec3 param_1, bvec3 param_2) {
    return vec3(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1], param_2[2] ? param_1[2] : param_0[2]);
}


struct GammaTransferParams {
  float G;
  float A;
  float B;
  float C;
  float D;
  float E;
  float F;
  uint padding;
};

struct ExternalTextureParams {
  uint numPlanes;
  uint doYuvToRgbConversionOnly;
  uint pad;
  uint pad_1;
  mat3x4 yuvToRgbConversionMatrix;
  GammaTransferParams gammaDecodeParams;
  GammaTransferParams gammaEncodeParams;
  mat3 gamutConversionMatrix;
  mat3x2 sampleTransform;
  mat3x2 loadTransform;
  vec2 samplePlane0RectMin;
  vec2 samplePlane0RectMax;
  vec2 samplePlane1RectMin;
  vec2 samplePlane1RectMax;
  uvec2 visibleSize;
  vec2 plane1CoordFactor;
};

struct ExternalTextureParams_std140 {
  uint numPlanes;
  uint doYuvToRgbConversionOnly;
  uint pad;
  uint pad_1;
  mat3x4 yuvToRgbConversionMatrix;
  GammaTransferParams gammaDecodeParams;
  GammaTransferParams gammaEncodeParams;
  mat3 gamutConversionMatrix;
  vec2 sampleTransform_0;
  vec2 sampleTransform_1;
  vec2 sampleTransform_2;
  vec2 loadTransform_0;
  vec2 loadTransform_1;
  vec2 loadTransform_2;
  vec2 samplePlane0RectMin;
  vec2 samplePlane0RectMax;
  vec2 samplePlane1RectMin;
  vec2 samplePlane1RectMax;
  uvec2 visibleSize;
  vec2 plane1CoordFactor;
};

layout(binding = 3, std140) uniform ext_tex_params_block_std140_ubo {
  ExternalTextureParams_std140 inner;
} ext_tex_params;

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  vec4 inner;
} prevent_dce;

vec3 gammaCorrection(vec3 v, GammaTransferParams params) {
  bvec3 cond = lessThan(abs(v), vec3(params.D));
  vec3 t = (sign(v) * ((params.C * abs(v)) + params.F));
  vec3 f = (sign(v) * (pow(((params.A * abs(v)) + params.B), vec3(params.G)) + params.E));
  return tint_select(f, t, cond);
}


vec4 textureSampleExternal(highp sampler2D plane0_smp, highp sampler2D plane1_smp, vec2 coord, ExternalTextureParams params) {
  vec2 modifiedCoords = (params.sampleTransform * vec3(coord, 1.0f));
  vec2 plane0_clamped = clamp(modifiedCoords, params.samplePlane0RectMin, params.samplePlane0RectMax);
  vec4 color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  if ((params.numPlanes == 1u)) {
    color = textureLod(plane0_smp, plane0_clamped, 0.0f).rgba;
  } else {
    vec2 plane1_clamped = clamp(modifiedCoords, params.samplePlane1RectMin, params.samplePlane1RectMax);
    color = vec4((vec4(textureLod(plane0_smp, plane0_clamped, 0.0f).r, textureLod(plane1_smp, plane1_clamped, 0.0f).rg, 1.0f) * params.yuvToRgbConversionMatrix), 1.0f);
  }
  if ((params.doYuvToRgbConversionOnly == 0u)) {
    color = vec4(gammaCorrection(color.rgb, params.gammaDecodeParams), color.a);
    color = vec4((params.gamutConversionMatrix * color.rgb), color.a);
    color = vec4(gammaCorrection(color.rgb, params.gammaEncodeParams), color.a);
  }
  return color;
}

uniform highp sampler2D arg_0_arg_1;
uniform highp sampler2D ext_tex_plane_1_arg_1;
ExternalTextureParams conv_ExternalTextureParams(ExternalTextureParams_std140 val) {
  return ExternalTextureParams(val.numPlanes, val.doYuvToRgbConversionOnly, val.pad, val.pad_1, val.yuvToRgbConversionMatrix, val.gammaDecodeParams, val.gammaEncodeParams, val.gamutConversionMatrix, mat3x2(val.sampleTransform_0, val.sampleTransform_1, val.sampleTransform_2), mat3x2(val.loadTransform_0, val.loadTransform_1, val.loadTransform_2), val.samplePlane0RectMin, val.samplePlane0RectMax, val.samplePlane1RectMin, val.samplePlane1RectMax, val.visibleSize, val.plane1CoordFactor);
}

vec4 textureSampleBaseClampToEdge_7c04e6() {
  vec2 arg_2 = vec2(1.0f);
  vec4 res = textureSampleExternal(arg_0_arg_1, ext_tex_plane_1_arg_1, arg_2, conv_ExternalTextureParams(ext_tex_params.inner));
  return res;
}

struct VertexOutput {
  vec4 pos;
  vec4 prevent_dce;
};

void fragment_main() {
  prevent_dce.inner = textureSampleBaseClampToEdge_7c04e6();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

vec3 tint_select(vec3 param_0, vec3 param_1, bvec3 param_2) {
    return vec3(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1], param_2[2] ? param_1[2] : param_0[2]);
}


struct GammaTransferParams {
  float G;
  float A;
  float B;
  float C;
  float D;
  float E;
  float F;
  uint padding;
};

struct ExternalTextureParams {
  uint numPlanes;
  uint doYuvToRgbConversionOnly;
  uint pad;
  uint pad_1;
  mat3x4 yuvToRgbConversionMatrix;
  GammaTransferParams gammaDecodeParams;
  GammaTransferParams gammaEncodeParams;
  mat3 gamutConversionMatrix;
  mat3x2 sampleTransform;
  mat3x2 loadTransform;
  vec2 samplePlane0RectMin;
  vec2 samplePlane0RectMax;
  vec2 samplePlane1RectMin;
  vec2 samplePlane1RectMax;
  uvec2 visibleSize;
  vec2 plane1CoordFactor;
};

struct ExternalTextureParams_std140 {
  uint numPlanes;
  uint doYuvToRgbConversionOnly;
  uint pad;
  uint pad_1;
  mat3x4 yuvToRgbConversionMatrix;
  GammaTransferParams gammaDecodeParams;
  GammaTransferParams gammaEncodeParams;
  mat3 gamutConversionMatrix;
  vec2 sampleTransform_0;
  vec2 sampleTransform_1;
  vec2 sampleTransform_2;
  vec2 loadTransform_0;
  vec2 loadTransform_1;
  vec2 loadTransform_2;
  vec2 samplePlane0RectMin;
  vec2 samplePlane0RectMax;
  vec2 samplePlane1RectMin;
  vec2 samplePlane1RectMax;
  uvec2 visibleSize;
  vec2 plane1CoordFactor;
};

layout(binding = 3, std140) uniform ext_tex_params_block_std140_ubo {
  ExternalTextureParams_std140 inner;
} ext_tex_params;

layout(binding = 0, std430) buffer prevent_dce_block_ssbo {
  vec4 inner;
} prevent_dce;

vec3 gammaCorrection(vec3 v, GammaTransferParams params) {
  bvec3 cond = lessThan(abs(v), vec3(params.D));
  vec3 t = (sign(v) * ((params.C * abs(v)) + params.F));
  vec3 f = (sign(v) * (pow(((params.A * abs(v)) + params.B), vec3(params.G)) + params.E));
  return tint_select(f, t, cond);
}


vec4 textureSampleExternal(highp sampler2D plane0_smp, highp sampler2D plane1_smp, vec2 coord, ExternalTextureParams params) {
  vec2 modifiedCoords = (params.sampleTransform * vec3(coord, 1.0f));
  vec2 plane0_clamped = clamp(modifiedCoords, params.samplePlane0RectMin, params.samplePlane0RectMax);
  vec4 color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  if ((params.numPlanes == 1u)) {
    color = textureLod(plane0_smp, plane0_clamped, 0.0f).rgba;
  } else {
    vec2 plane1_clamped = clamp(modifiedCoords, params.samplePlane1RectMin, params.samplePlane1RectMax);
    color = vec4((vec4(textureLod(plane0_smp, plane0_clamped, 0.0f).r, textureLod(plane1_smp, plane1_clamped, 0.0f).rg, 1.0f) * params.yuvToRgbConversionMatrix), 1.0f);
  }
  if ((params.doYuvToRgbConversionOnly == 0u)) {
    color = vec4(gammaCorrection(color.rgb, params.gammaDecodeParams), color.a);
    color = vec4((params.gamutConversionMatrix * color.rgb), color.a);
    color = vec4(gammaCorrection(color.rgb, params.gammaEncodeParams), color.a);
  }
  return color;
}

uniform highp sampler2D arg_0_arg_1;
uniform highp sampler2D ext_tex_plane_1_arg_1;
ExternalTextureParams conv_ExternalTextureParams(ExternalTextureParams_std140 val) {
  return ExternalTextureParams(val.numPlanes, val.doYuvToRgbConversionOnly, val.pad, val.pad_1, val.yuvToRgbConversionMatrix, val.gammaDecodeParams, val.gammaEncodeParams, val.gamutConversionMatrix, mat3x2(val.sampleTransform_0, val.sampleTransform_1, val.sampleTransform_2), mat3x2(val.loadTransform_0, val.loadTransform_1, val.loadTransform_2), val.samplePlane0RectMin, val.samplePlane0RectMax, val.samplePlane1RectMin, val.samplePlane1RectMax, val.visibleSize, val.plane1CoordFactor);
}

vec4 textureSampleBaseClampToEdge_7c04e6() {
  vec2 arg_2 = vec2(1.0f);
  vec4 res = textureSampleExternal(arg_0_arg_1, ext_tex_plane_1_arg_1, arg_2, conv_ExternalTextureParams(ext_tex_params.inner));
  return res;
}

struct VertexOutput {
  vec4 pos;
  vec4 prevent_dce;
};

void compute_main() {
  prevent_dce.inner = textureSampleBaseClampToEdge_7c04e6();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
#version 310 es

vec3 tint_select(vec3 param_0, vec3 param_1, bvec3 param_2) {
    return vec3(param_2[0] ? param_1[0] : param_0[0], param_2[1] ? param_1[1] : param_0[1], param_2[2] ? param_1[2] : param_0[2]);
}


layout(location = 0) flat out vec4 prevent_dce_1;
struct GammaTransferParams {
  float G;
  float A;
  float B;
  float C;
  float D;
  float E;
  float F;
  uint padding;
};

struct ExternalTextureParams {
  uint numPlanes;
  uint doYuvToRgbConversionOnly;
  uint pad;
  uint pad_1;
  mat3x4 yuvToRgbConversionMatrix;
  GammaTransferParams gammaDecodeParams;
  GammaTransferParams gammaEncodeParams;
  mat3 gamutConversionMatrix;
  mat3x2 sampleTransform;
  mat3x2 loadTransform;
  vec2 samplePlane0RectMin;
  vec2 samplePlane0RectMax;
  vec2 samplePlane1RectMin;
  vec2 samplePlane1RectMax;
  uvec2 visibleSize;
  vec2 plane1CoordFactor;
};

struct ExternalTextureParams_std140 {
  uint numPlanes;
  uint doYuvToRgbConversionOnly;
  uint pad;
  uint pad_1;
  mat3x4 yuvToRgbConversionMatrix;
  GammaTransferParams gammaDecodeParams;
  GammaTransferParams gammaEncodeParams;
  mat3 gamutConversionMatrix;
  vec2 sampleTransform_0;
  vec2 sampleTransform_1;
  vec2 sampleTransform_2;
  vec2 loadTransform_0;
  vec2 loadTransform_1;
  vec2 loadTransform_2;
  vec2 samplePlane0RectMin;
  vec2 samplePlane0RectMax;
  vec2 samplePlane1RectMin;
  vec2 samplePlane1RectMax;
  uvec2 visibleSize;
  vec2 plane1CoordFactor;
};

layout(binding = 3, std140) uniform ext_tex_params_block_std140_ubo {
  ExternalTextureParams_std140 inner;
} ext_tex_params;

vec3 gammaCorrection(vec3 v, GammaTransferParams params) {
  bvec3 cond = lessThan(abs(v), vec3(params.D));
  vec3 t = (sign(v) * ((params.C * abs(v)) + params.F));
  vec3 f = (sign(v) * (pow(((params.A * abs(v)) + params.B), vec3(params.G)) + params.E));
  return tint_select(f, t, cond);
}


vec4 textureSampleExternal(highp sampler2D plane0_smp, highp sampler2D plane1_smp, vec2 coord, ExternalTextureParams params) {
  vec2 modifiedCoords = (params.sampleTransform * vec3(coord, 1.0f));
  vec2 plane0_clamped = clamp(modifiedCoords, params.samplePlane0RectMin, params.samplePlane0RectMax);
  vec4 color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  if ((params.numPlanes == 1u)) {
    color = textureLod(plane0_smp, plane0_clamped, 0.0f).rgba;
  } else {
    vec2 plane1_clamped = clamp(modifiedCoords, params.samplePlane1RectMin, params.samplePlane1RectMax);
    color = vec4((vec4(textureLod(plane0_smp, plane0_clamped, 0.0f).r, textureLod(plane1_smp, plane1_clamped, 0.0f).rg, 1.0f) * params.yuvToRgbConversionMatrix), 1.0f);
  }
  if ((params.doYuvToRgbConversionOnly == 0u)) {
    color = vec4(gammaCorrection(color.rgb, params.gammaDecodeParams), color.a);
    color = vec4((params.gamutConversionMatrix * color.rgb), color.a);
    color = vec4(gammaCorrection(color.rgb, params.gammaEncodeParams), color.a);
  }
  return color;
}

uniform highp sampler2D arg_0_arg_1;
uniform highp sampler2D ext_tex_plane_1_arg_1;
ExternalTextureParams conv_ExternalTextureParams(ExternalTextureParams_std140 val) {
  return ExternalTextureParams(val.numPlanes, val.doYuvToRgbConversionOnly, val.pad, val.pad_1, val.yuvToRgbConversionMatrix, val.gammaDecodeParams, val.gammaEncodeParams, val.gamutConversionMatrix, mat3x2(val.sampleTransform_0, val.sampleTransform_1, val.sampleTransform_2), mat3x2(val.loadTransform_0, val.loadTransform_1, val.loadTransform_2), val.samplePlane0RectMin, val.samplePlane0RectMax, val.samplePlane1RectMin, val.samplePlane1RectMax, val.visibleSize, val.plane1CoordFactor);
}

vec4 textureSampleBaseClampToEdge_7c04e6() {
  vec2 arg_2 = vec2(1.0f);
  vec4 res = textureSampleExternal(arg_0_arg_1, ext_tex_plane_1_arg_1, arg_2, conv_ExternalTextureParams(ext_tex_params.inner));
  return res;
}

struct VertexOutput {
  vec4 pos;
  vec4 prevent_dce;
};

VertexOutput vertex_main() {
  VertexOutput tint_symbol = VertexOutput(vec4(0.0f, 0.0f, 0.0f, 0.0f), vec4(0.0f, 0.0f, 0.0f, 0.0f));
  tint_symbol.pos = vec4(0.0f);
  tint_symbol.prevent_dce = textureSampleBaseClampToEdge_7c04e6();
  return tint_symbol;
}

void main() {
  gl_PointSize = 1.0;
  VertexOutput inner_result = vertex_main();
  gl_Position = inner_result.pos;
  prevent_dce_1 = inner_result.prevent_dce;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}

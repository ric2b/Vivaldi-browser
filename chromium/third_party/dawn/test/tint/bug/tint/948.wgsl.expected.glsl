#version 310 es
precision highp float;
precision highp int;


struct LeftOver {
  float time;
  uint padding;
  uint tint_pad_0;
  uint tint_pad_1;
  mat4 worldViewProjection;
  vec2 outputSize;
  vec2 stageSize;
  vec2 spriteMapSize;
  float stageScale;
  float spriteCount;
  vec3 colorMul;
  uint tint_pad_2;
};

struct main_out {
  vec4 glFragColor_1;
};

layout(binding = 9, std140)
uniform x_20_block_1_ubo {
  LeftOver inner;
} v;
vec2 tUV = vec2(0.0f);
float mt = 0.0f;
vec4 glFragColor = vec4(0.0f);
vec2 tileID_1 = vec2(0.0f);
vec2 levelUnits = vec2(0.0f);
vec2 stageUnits_1 = vec2(0.0f);
vec3 vPosition = vec3(0.0f);
vec2 vUV = vec2(0.0f);
uniform highp sampler2D frameMapTexture_frameMapSampler;
uniform highp sampler2D tileMapsTexture1_tileMapsSampler;
uniform highp sampler2D tileMapsTexture0_tileMapsSampler;
uniform highp sampler2D animationMapTexture_animationMapSampler;
uniform highp sampler2D spriteSheetTexture_spriteSheetSampler;
layout(location = 2) in vec2 tint_symbol_loc2_Input;
layout(location = 5) in vec2 tint_symbol_loc5_Input;
layout(location = 4) in vec2 tint_symbol_loc4_Input;
layout(location = 3) in vec2 tint_symbol_loc3_Input;
layout(location = 0) in vec3 tint_symbol_loc0_Input;
layout(location = 1) in vec2 tint_symbol_loc1_Input;
layout(location = 0) out vec4 tint_symbol_loc0_Output;
mat4 getFrameData_f1_(inout float frameID) {
  float fX = 0.0f;
  float x_15 = frameID;
  float x_25 = v.inner.spriteCount;
  fX = (x_15 / x_25);
  float x_37 = fX;
  vec2 v_1 = vec2(x_37, 0.0f);
  vec4 x_40 = texture(frameMapTexture_frameMapSampler, v_1, clamp(0.0f, -16.0f, 15.9899997711181640625f));
  float x_44 = fX;
  vec2 v_2 = vec2(x_44, 0.25f);
  vec4 x_47 = texture(frameMapTexture_frameMapSampler, v_2, clamp(0.0f, -16.0f, 15.9899997711181640625f));
  float x_51 = fX;
  vec2 v_3 = vec2(x_51, 0.5f);
  vec4 x_54 = texture(frameMapTexture_frameMapSampler, v_3, clamp(0.0f, -16.0f, 15.9899997711181640625f));
  vec4 v_4 = vec4(x_40[0u], x_40[1u], x_40[2u], x_40[3u]);
  vec4 v_5 = vec4(x_47[0u], x_47[1u], x_47[2u], x_47[3u]);
  return mat4(v_4, v_5, vec4(x_54[0u], x_54[1u], x_54[2u], x_54[3u]), vec4(0.0f));
}
float tint_float_modulo(float x, float y) {
  return (x - (y * trunc((x / y))));
}
void main_1() {
  vec4 color = vec4(0.0f);
  vec2 tileUV = vec2(0.0f);
  vec2 tileID = vec2(0.0f);
  vec2 sheetUnits = vec2(0.0f);
  float spriteUnits = 0.0f;
  vec2 stageUnits = vec2(0.0f);
  int i = 0;
  float frameID_1 = 0.0f;
  vec4 animationData = vec4(0.0f);
  float f = 0.0f;
  mat4 frameData = mat4(vec4(0.0f), vec4(0.0f), vec4(0.0f), vec4(0.0f));
  float param = 0.0f;
  vec2 frameSize = vec2(0.0f);
  vec2 offset_1 = vec2(0.0f);
  vec2 ratio = vec2(0.0f);
  vec4 nc = vec4(0.0f);
  float alpha = 0.0f;
  vec3 mixed = vec3(0.0f);
  color = vec4(0.0f);
  vec2 x_86 = tUV;
  tileUV = fract(x_86);
  float x_91 = tileUV.y;
  tileUV[1u] = (1.0f - x_91);
  vec2 x_95 = tUV;
  tileID = floor(x_95);
  vec2 x_101 = v.inner.spriteMapSize;
  sheetUnits = (vec2(1.0f) / x_101);
  float x_106 = v.inner.spriteCount;
  spriteUnits = (1.0f / x_106);
  vec2 x_111 = v.inner.stageSize;
  stageUnits = (vec2(1.0f) / x_111);
  i = 0;
  {
    while(true) {
      int x_122 = i;
      if ((x_122 < 2)) {
      } else {
        break;
      }
      int x_126 = i;
      switch(x_126) {
        case 1:
        {
          vec2 x_150 = tileID;
          vec2 x_154 = v.inner.stageSize;
          vec4 x_156 = texture(tileMapsTexture1_tileMapsSampler, ((x_150 + vec2(0.5f)) / x_154), clamp(0.0f, -16.0f, 15.9899997711181640625f));
          frameID_1 = x_156[0u];
          break;
        }
        case 0:
        {
          vec2 x_136 = tileID;
          vec2 x_140 = v.inner.stageSize;
          vec4 x_142 = texture(tileMapsTexture0_tileMapsSampler, ((x_136 + vec2(0.5f)) / x_140), clamp(0.0f, -16.0f, 15.9899997711181640625f));
          frameID_1 = x_142[0u];
          break;
        }
        default:
        {
          break;
        }
      }
      float x_166 = frameID_1;
      float x_169 = v.inner.spriteCount;
      vec2 v_6 = vec2(((x_166 + 0.5f) / x_169), 0.0f);
      vec4 x_172 = texture(animationMapTexture_animationMapSampler, v_6, clamp(0.0f, -16.0f, 15.9899997711181640625f));
      animationData = x_172;
      float x_174 = animationData.y;
      if ((x_174 > 0.0f)) {
        float x_181 = v.inner.time;
        float x_184 = animationData.z;
        mt = tint_float_modulo((x_181 * x_184), 1.0f);
        f = 0.0f;
        {
          while(true) {
            float x_193 = f;
            if ((x_193 < 8.0f)) {
            } else {
              break;
            }
            float x_197 = animationData.y;
            float x_198 = mt;
            if ((x_197 > x_198)) {
              float x_203 = animationData.x;
              frameID_1 = x_203;
              break;
            }
            float x_208 = frameID_1;
            float x_211 = v.inner.spriteCount;
            float x_214 = f;
            vec4 x_217 = vec4(0.0f);
            animationData = x_217;
            {
              float x_218 = f;
              f = (x_218 + 1.0f);
            }
            continue;
          }
        }
      }
      float x_222 = frameID_1;
      param = (x_222 + 0.5f);
      mat4 x_225 = getFrameData_f1_(param);
      frameData = x_225;
      vec4 x_228 = frameData[0];
      vec2 x_231 = v.inner.spriteMapSize;
      frameSize = (vec2(x_228[3u], x_228[2u]) / x_231);
      vec4 x_235 = frameData[0];
      vec2 x_237 = sheetUnits;
      offset_1 = (vec2(x_235[0u], x_235[1u]) * x_237);
      vec4 x_241 = frameData[2];
      vec4 x_244 = frameData[0];
      vec2 v_7 = vec2(x_241[0u], x_241[1u]);
      ratio = (v_7 / vec2(x_244[3u], x_244[2u]));
      float x_248 = frameData[2].z;
      if ((x_248 == 1.0f)) {
        vec2 x_252 = tileUV;
        tileUV = vec2(x_252[1u], x_252[0u]);
      }
      int x_254 = i;
      if ((x_254 == 0)) {
        vec2 x_263 = tileUV;
        vec2 x_264 = frameSize;
        vec2 x_266 = offset_1;
        vec4 x_268 = texture(spriteSheetTexture_spriteSheetSampler, ((x_263 * x_264) + x_266));
        color = x_268;
      } else {
        vec2 x_274 = tileUV;
        vec2 x_275 = frameSize;
        vec2 x_277 = offset_1;
        vec4 x_279 = texture(spriteSheetTexture_spriteSheetSampler, ((x_274 * x_275) + x_277));
        nc = x_279;
        float x_283 = color.w;
        float x_285 = nc.w;
        alpha = min((x_283 + x_285), 1.0f);
        vec4 x_290 = color;
        vec4 x_292 = nc;
        float x_295 = nc.w;
        vec3 v_8 = vec3(x_290[0u], x_290[1u], x_290[2u]);
        vec3 v_9 = vec3(x_292[0u], x_292[1u], x_292[2u]);
        mixed = mix(v_8, v_9, vec3(x_295, x_295, x_295));
        vec3 x_298 = mixed;
        float x_299 = alpha;
        color = vec4(x_298[0u], x_298[1u], x_298[2u], x_299);
      }
      {
        int x_304 = i;
        i = (x_304 + 1);
      }
      continue;
    }
  }
  vec3 x_310 = v.inner.colorMul;
  vec4 x_311 = color;
  vec3 x_313 = (vec3(x_311[0u], x_311[1u], x_311[2u]) * x_310);
  vec4 x_314 = color;
  color = vec4(x_313[0u], x_313[1u], x_313[2u], x_314[3u]);
  vec4 x_318 = color;
  glFragColor = x_318;
}
main_out tint_symbol_inner(vec2 tUV_param, vec2 tileID_1_param, vec2 levelUnits_param, vec2 stageUnits_1_param, vec3 vPosition_param, vec2 vUV_param) {
  tUV = tUV_param;
  tileID_1 = tileID_1_param;
  levelUnits = levelUnits_param;
  stageUnits_1 = stageUnits_1_param;
  vPosition = vPosition_param;
  vUV = vUV_param;
  main_1();
  return main_out(glFragColor);
}
void main() {
  tint_symbol_loc0_Output = tint_symbol_inner(tint_symbol_loc2_Input, tint_symbol_loc5_Input, tint_symbol_loc4_Input, tint_symbol_loc3_Input, tint_symbol_loc0_Input, tint_symbol_loc1_Input).glFragColor_1;
}

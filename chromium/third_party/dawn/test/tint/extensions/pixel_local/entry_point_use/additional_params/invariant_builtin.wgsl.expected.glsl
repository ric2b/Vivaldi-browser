SKIP: INVALID


enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
  b : i32,
  c : f32,
}

var<pixel_local> P : PixelLocal;

@fragment
fn f(@invariant @builtin(position) pos : vec4f) {
  P.a += u32(pos.x);
}

Failed to generate: <dawn>/test/tint/extensions/pixel_local/entry_point_use/additional_params/invariant_builtin.wgsl:2:8 error: GLSL backend does not support extension 'chromium_experimental_pixel_local'
enable chromium_experimental_pixel_local;
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


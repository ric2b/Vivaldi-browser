SKIP: INVALID


enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
  b : i32,
  c : f32,
}

var<pixel_local> P : PixelLocal;

struct In {
  @location(0)
  uv : vec4f,
}

@fragment
fn f(@builtin(position) pos : vec4f, tint_symbol : In) {
  P.a += (u32(pos.x) + u32(tint_symbol.uv.x));
}

Failed to generate: <dawn>/test/tint/extensions/pixel_local/entry_point_use/additional_params/builtin_and_location_in_struct.wgsl:2:8 error: GLSL backend does not support extension 'chromium_experimental_pixel_local'
enable chromium_experimental_pixel_local;
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


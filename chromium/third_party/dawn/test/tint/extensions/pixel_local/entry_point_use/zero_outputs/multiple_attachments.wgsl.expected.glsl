SKIP: INVALID


enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
  b : i32,
  c : f32,
}

var<pixel_local> P : PixelLocal;

@fragment
fn f() {
  P.a += 42;
}

Failed to generate: <dawn>/test/tint/extensions/pixel_local/entry_point_use/zero_outputs/multiple_attachments.wgsl:2:8 error: GLSL backend does not support extension 'chromium_experimental_pixel_local'
enable chromium_experimental_pixel_local;
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


SKIP: FAILED


enable chromium_internal_input_attachments;

@input_attachment_index(3) @group(1) @binding(0) var arg_0 : input_attachment<f32>;

fn inputAttachmentLoad_c38b2f() {
  var res : vec4<f32> = inputAttachmentLoad(arg_0);
  prevent_dce = res;
}

@group(2) @binding(0) var<storage, read_write> prevent_dce : vec4<f32>;

@fragment
fn fragment_main() {
  inputAttachmentLoad_c38b2f();
}

Failed to generate: <dawn>/test/tint/builtins/gen/var/inputAttachmentLoad/c38b2f.wgsl:38:8 error: HLSL backend does not support extension 'chromium_internal_input_attachments'
enable chromium_internal_input_attachments;
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


SKIP: FAILED


enable chromium_internal_input_attachments;

@input_attachment_index(3) @group(1) @binding(0) var arg_0 : input_attachment<i32>;

fn inputAttachmentLoad_315bf5() {
  var res : vec4<i32> = inputAttachmentLoad(arg_0);
  prevent_dce = res;
}

@group(2) @binding(0) var<storage, read_write> prevent_dce : vec4<i32>;

@fragment
fn fragment_main() {
  inputAttachmentLoad_315bf5();
}

Failed to generate: <dawn>/test/tint/builtins/gen/literal/inputAttachmentLoad/315bf5.wgsl:38:8 error: HLSL backend does not support extension 'chromium_internal_input_attachments'
enable chromium_internal_input_attachments;
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


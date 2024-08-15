#include "extensions/renderer/guest_view/guest_view_internal_custom_bindings.h"

#include "app/vivaldi_apptools.h"

namespace extensions {

void GuestViewInternalCustomBindings::IsVivaldi(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(vivaldi::IsVivaldiRunning());
}

}  // namespace extensions

// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/renderer/vivaldi_script_dispatcher.h"

#include "app/vivaldi_apptools.h"
#include "extensions/common/extension_features.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"

#include "vivaldi/grit/vivaldi_extension_resources.h"

namespace vivaldi {

// This handles the "vivaldi." root API object in script contexts.
class VivaldiNativeHandler : public extensions::ObjectBackedNativeHandler {
 public:
  explicit VivaldiNativeHandler(extensions::ScriptContext* context)
      : extensions::ObjectBackedNativeHandler(context) {}

  void AddRoutes() override {
    RouteHandlerFunction("GetVivaldi",
                         base::BindRepeating(&VivaldiNativeHandler::GetVivaldi,
                                             base::Unretained(this)));
  }

  void GetVivaldi(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::String> vivaldi_string(
        v8::String::NewFromUtf8(context()->isolate(), "vivaldi",
                                v8::NewStringType::kInternalized)
            .ToLocalChecked());
    v8::Local<v8::Object> global(context()->v8_context()->Global());
    v8::Local<v8::Value> vivaldi(
        global->Get(context()->v8_context(), vivaldi_string).ToLocalChecked());
    if (vivaldi->IsUndefined()) {
      vivaldi = v8::Object::New(context()->isolate());
      global->Set(context()->v8_context(), vivaldi_string, vivaldi).ToChecked();
    }
    args.GetReturnValue().Set(vivaldi);
  }
};

void VivaldiRegisterNativeHandler(extensions::ModuleSystem* module_system,
                                  extensions::ScriptContext* context) {
  module_system->RegisterNativeHandler(
      "vivaldi", std::make_unique<VivaldiNativeHandler>(context));
}

// Called by Dispatcher::GetJsResources()
void VivaldiAddScriptResources(extensions::ResourceBundleSourceMap* source_map) {
  source_map->RegisterSource("webViewPrivateImpl", IDR_WEB_VIEW_PRIVATE_API_IMPL_JS);
  source_map->RegisterSource("webViewPrivateMethods", IDR_WEB_VIEW_PRIVATE_API_METHODS_JS);

  source_map->RegisterSource("webViewEventsPrivate", IDR_WEB_VIEW_PRIVATE_EVENTS_JS);

  source_map->RegisterSource("webViewAttributesPrivate", IDR_WEB_VIEW_PRIVATE_ATTRIBUTES_JS);
  source_map->RegisterSource("webViewConstantsPrivate", IDR_WEB_VIEW_PRIVATE_CONSTANTS_JS);
}

// Called by Dispatcher::RequireGuestViewModules()
void VivaldiAddRequiredModules(extensions::ScriptContext* context,
                               extensions::ModuleSystem* module_system) {
  // Require WebView.
  if (context->GetAvailability("webViewInternal").is_available() &&
      vivaldi::IsVivaldiRunning()) {
    module_system->Require("webViewPrivateImpl");
  }
}

}  // namespace vivaldi

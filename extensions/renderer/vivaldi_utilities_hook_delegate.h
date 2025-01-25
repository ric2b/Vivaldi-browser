// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_RENDERER_VIVALDI_UTILITIES_HOOK_DELEGATE_H_
#define EXTENSIONS_RENDERER_VIVALDI_UTILITIES_HOOK_DELEGATE_H_

#include <vector>

#include "extensions/renderer/bindings/api_binding_hooks_delegate.h"
#include "v8/include/v8.h"

namespace extensions {

// The custom hooks for the chrome.extension API.
class VivaldiUtilitiesHookDelegate : public APIBindingHooksDelegate {
 public:
  explicit VivaldiUtilitiesHookDelegate();
  ~VivaldiUtilitiesHookDelegate() override;
  VivaldiUtilitiesHookDelegate(const VivaldiUtilitiesHookDelegate&) = delete;
  VivaldiUtilitiesHookDelegate& operator=(const VivaldiUtilitiesHookDelegate&) =
      delete;

  // APIBindingHooksDelegate:
  APIBindingHooks::RequestResult HandleRequest(
      const std::string& method_name,
      const APISignature* signature,
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>* arguments,
      const APITypeReferenceMap& refs) override;

 private:
  // Request handlers for the corresponding API methods.
  APIBindingHooks::RequestResult HandleGetUrlFragments(
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>& arguments);
  APIBindingHooks::RequestResult HandleGetVersion(
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>& arguments);
  APIBindingHooks::RequestResult HandleIsUrlValid(
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>& arguments);
  APIBindingHooks::RequestResult HandleUrlToThumbnailText(
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>& arguments);
  APIBindingHooks::RequestResult HandleIsRTL(
      v8::Local<v8::Context> context,
      v8::LocalVector<v8::Value>& arguments);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_VIVALDI_UTILITIES_HOOK_DELEGATE_H_

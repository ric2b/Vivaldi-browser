// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/renderer/vivaldi_utilities_hook_delegate.h"

#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/version_info/version_info.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/url_pattern.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_constants.h"

#include "app/vivaldi_version_info.h"

namespace extensions {

namespace {

using RequestResult = APIBindingHooks::RequestResult;

const char kDevToolsLegacyScheme[] = "chrome-devtools";
const char kDevToolsScheme[] = "devtools";

}  // namespace

VivaldiUtilitiesHookDelegate::VivaldiUtilitiesHookDelegate() = default;
VivaldiUtilitiesHookDelegate::~VivaldiUtilitiesHookDelegate() = default;

// This is based on RuntimeHooksDelegate::HandleRequest().
RequestResult VivaldiUtilitiesHookDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& refs) {
  using Handler = RequestResult (VivaldiUtilitiesHookDelegate::*)(
      v8::Local<v8::Context>, const std::vector<v8::Local<v8::Value>>&);
  static struct {
    Handler handler;
    base::StringPiece method;
  } kHandlers[] = {
      {&VivaldiUtilitiesHookDelegate::HandleGetUrlFragments,
       "utilities.getUrlFragments"},
      {&VivaldiUtilitiesHookDelegate::HandleGetVersion, "utilities.getVersion"},
      {&VivaldiUtilitiesHookDelegate::HandleIsUrlValid, "utilities.isUrlValid"},
  };

  Handler handler = nullptr;
  for (const auto& handler_entry : kHandlers) {
    if (handler_entry.method == method_name) {
      handler = handler_entry.handler;
      break;
    }
  }

  if (!handler)
    return RequestResult(RequestResult::NOT_HANDLED);

  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }

  return (this->*handler)(std::move(context), *parse_result.arguments);
}

RequestResult VivaldiUtilitiesHookDelegate::HandleGetUrlFragments(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(1u, arguments.size());
  DCHECK(arguments[0]->IsString());
  v8::Isolate* isolate = context->GetIsolate();
  std::string url_string = gin::V8ToString(isolate, arguments[0]);

  GURL url(url_string);
  url::Parsed parsed;
  if (url.is_valid()) {
    if (url.SchemeIsFile()) {
      ParseFileURL(url.spec().c_str(), url.spec().length(), &parsed);
    } else {
      ParseStandardURL(url.spec().c_str(), url.spec().length(), &parsed);
      if (url.host().empty() && parsed.host.end() > 0) {
        // Of the type "javascript:..."
        ParsePathURL(url.spec().c_str(), url.spec().length(), false, &parsed);
      }
    }
  }

  v8::Local<v8::Object> fragments = v8::Object::New(isolate);

  auto set_fragment = [&](base::StringPiece key, base::StringPiece value) {
    fragments
        ->Set(context, gin::StringToV8(isolate, key),
              gin::StringToV8(isolate, value))
        .ToChecked();
  };

  if (parsed.scheme.is_valid()) {
    set_fragment("scheme", url.scheme_piece());
  }

  if (parsed.username.is_valid()) {
    set_fragment("username", url.username_piece());
  }

  if (parsed.password.is_valid()) {
    set_fragment("password", url.password_piece());
  }

  if (parsed.host.is_valid()) {
    set_fragment("host", url.host_piece());
  }

  if (parsed.port.is_valid()) {
    set_fragment("port", url.port_piece());
  }

  if (parsed.path.is_valid()) {
    set_fragment("path", url.path_piece());
  }

  if (parsed.query.is_valid()) {
    set_fragment("query", url.query_piece());
  }

  if (parsed.ref.is_valid()) {
    set_fragment("ref", url.ref_piece());
  }

  if (parsed.host.is_valid()) {
    DomainInfo info = GetDomainInfo(url);
    base::StringPiece tld(info.domain_and_registry);
    if (!info.domain_without_registry.empty()) {
      tld = tld.substr(info.domain_without_registry.length() + 1);
    }
    set_fragment("tld", tld);
  }

  RequestResult result(RequestResult::HANDLED);
  result.return_value = std::move(fragments);
  return result;
}

RequestResult VivaldiUtilitiesHookDelegate::HandleGetVersion(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> version_object = v8::Object::New(isolate);
  version_object
      ->Set(context, gin::StringToV8(isolate, "vivaldiVersion"),
            gin::StringToV8(isolate, ::vivaldi::GetVivaldiVersionString()))
      .ToChecked();
  version_object
      ->Set(context, gin::StringToV8(isolate, "chromiumVersion"),
            gin::StringToV8(isolate, version_info::GetVersionNumber()))
      .ToChecked();

  RequestResult result(RequestResult::HANDLED);
  result.return_value = std::move(version_object);
  return result;
}

namespace {

bool DoesBrowserHandleUrl(const GURL& url) {
  base::StringPiece scheme = url.scheme_piece();
  if (URLPattern::IsValidSchemeForExtensions(scheme))
    return true;
  static const base::StringPiece extra_schemes[] = {
      url::kJavaScriptScheme,     url::kDataScheme,      url::kMailToScheme,
      content::kViewSourceScheme, kDevToolsLegacyScheme, kDevToolsScheme,
  };
  for (base::StringPiece extra_scheme : extra_schemes) {
    if (scheme == extra_scheme)
      return true;
  }

  // For about: urls only the blank page is supported.
  return url.IsAboutBlank();
}

}   //namespace
RequestResult VivaldiUtilitiesHookDelegate::HandleIsUrlValid(
    v8::Local<v8::Context> context,
    const std::vector<v8::Local<v8::Value>>& arguments) {
  DCHECK_EQ(1u, arguments.size());
  DCHECK(arguments[0]->IsString());
  v8::Isolate* isolate = context->GetIsolate();
  std::string url_string = gin::V8ToString(isolate, arguments[0]);

  GURL url(url_string);
  bool url_valid = url.is_valid();
  bool is_browser_url = DoesBrowserHandleUrl(url);
  std::string scheme_parsed = url.scheme();

  // GURL::spec() can only be called when url is valid.
  std::string normalized_url = url.is_valid() ? url.spec() : std::string();

  v8::Local<v8::Object> result_object = v8::Object::New(isolate);
  result_object
      ->Set(context, gin::StringToV8(isolate, "urlValid"),
            v8::Boolean::New(isolate, url_valid))
      .ToChecked();
  result_object
      ->Set(context, gin::StringToV8(isolate, "isBrowserUrl"),
            v8::Boolean::New(isolate, is_browser_url))
      .ToChecked();
  result_object
      ->Set(context, gin::StringToV8(isolate, "schemeParsed"),
            gin::StringToV8(isolate, scheme_parsed))
      .ToChecked();
  result_object
      ->Set(context, gin::StringToV8(isolate, "normalizedUrl"),
            gin::StringToV8(isolate, normalized_url))
      .ToChecked();
  RequestResult result(RequestResult::HANDLED);
  result.return_value = std::move(result_object);
  return result;
}

}  // namespace extensions

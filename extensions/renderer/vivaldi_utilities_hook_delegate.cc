// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/renderer/vivaldi_utilities_hook_delegate.h"

#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/url_formatter/elide_url.h"
#include "components/version_info/version_info.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/url_pattern.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
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
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  using Handler = RequestResult (VivaldiUtilitiesHookDelegate::*)(
      v8::Local<v8::Context>, v8::LocalVector<v8::Value> &);
  static struct {
    Handler handler;
    std::string_view method;
  } kHandlers[] = {
      {&VivaldiUtilitiesHookDelegate::HandleGetUrlFragments,
       "utilities.getUrlFragments"},
      {&VivaldiUtilitiesHookDelegate::HandleGetVersion, "utilities.getVersion"},
      {&VivaldiUtilitiesHookDelegate::HandleIsUrlValid, "utilities.isUrlValid"},
      {&VivaldiUtilitiesHookDelegate::HandleUrlToThumbnailText,
       "utilities.urlToThumbnailText"},
      {&VivaldiUtilitiesHookDelegate::HandleIsRTL, "utilities.isRTL"},
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
    v8::LocalVector<v8::Value>& arguments) {
  DCHECK_EQ(1u, arguments.size());
  DCHECK(arguments[0]->IsString());
  v8::Isolate* isolate = context->GetIsolate();
  std::string url_string = gin::V8ToString(isolate, arguments[0]);

  GURL url(url_string);
  url::Parsed parsed;
  url::Parsed parsed_unicode;
  std::u16string formatted_url;

  if (url.is_valid()) {
    if (url.SchemeIsFile()) {
      parsed = url::ParseFileURL(std::string_view(url.spec()));
    } else {
      ParseStandardURL(url.spec().c_str(), url.spec().length(), &parsed);
      if (url.host().empty() && parsed.host.end() > 0) {
        // Of the type "javascript:..."
        ParsePathURL(url.spec().c_str(), url.spec().length(), false, &parsed);
      }
    }
  }

  v8::Local<v8::Object> fragments = v8::Object::New(isolate);

  auto set_fragment = [&](std::string_view key, std::string_view value) {
    fragments
        ->Set(context, gin::StringToV8(isolate, key),
              gin::StringToV8(isolate, value))
        .ToChecked();
  };
  auto set_fragment16 = [&](std::string_view key,
                            const std::u16string& value) {
    fragments
        ->Set(context, gin::StringToV8(isolate, key),
              gin::StringToSymbol(isolate, value))
        .ToChecked();
  };
  // Set a url component for security display
  auto set_fragment16_for_sd = [&](const std::u16string& url,
                                   std::string_view key, url::Component comp) {
    std::u16string value;
    if (comp.len > 0) {
      value = std::u16string(&url[comp.begin], comp.len);
    }
    fragments
        ->Set(context, gin::StringToV8(isolate, key),
              gin::StringToSymbol(isolate, value))
        .ToChecked();
  };
  if (url.is_valid()) {
    formatted_url = url_formatter::FormatUrl(
        url, url_formatter::kFormatUrlOmitNothing, base::UnescapeRule::NORMAL,
        &parsed_unicode, nullptr, nullptr);

    set_fragment16("urlForSecurityDisplay", formatted_url);
  }
  if (parsed_unicode.Length()) {
    set_fragment16_for_sd(formatted_url, "hostForSecurityDisplay",
                          parsed_unicode.host);
    set_fragment16_for_sd(formatted_url, "pathForSecurityDisplay",
                          parsed_unicode.path);
    set_fragment16_for_sd(formatted_url, "queryForSecurityDisplay",
                          parsed_unicode.query);
    set_fragment16_for_sd(formatted_url, "refForSecurityDisplay",
                          parsed_unicode.ref);

    lookalikes::DomainInfo info = lookalikes::GetDomainInfo(url);
    std::u16string tld(info.idn_result.result);
    if (!tld.empty()) {
      // tld variable now contains topleveldomain.tld, so first dot is the start
      // of the real tld.
      size_t dot = tld.find_first_of('.');
      if (dot != std::u16string::npos) {
        tld = tld.substr(dot + 1);
      }
    }
    set_fragment16("tldForSecurityDisplay", tld);
  }

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
    lookalikes::DomainInfo info = lookalikes::GetDomainInfo(url);
    std::string_view tld(info.domain_and_registry);
    if (!info.domain_without_registry.empty()) {
      tld = tld.substr(info.domain_without_registry.length() + 1);
    }
    set_fragment("tld", tld);
  }

  RequestResult result(RequestResult::HANDLED);
  result.return_value = std::move(fragments);
  return result;
}

RequestResult VivaldiUtilitiesHookDelegate::HandleUrlToThumbnailText(
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value> &arguments) {
  constexpr char kChrome[] = "chrome";
  DCHECK_EQ(1u, arguments.size());
  DCHECK(arguments[0]->IsString());
  v8::Isolate* isolate = context->GetIsolate();
  std::string url_string = gin::V8ToString(isolate, arguments[0]);

  RequestResult result(RequestResult::HANDLED);
  GURL url(url_string);
  if (!url.is_valid()) {
    result.return_value = arguments[0];
  } else if (url.scheme().substr(0, 5) == kChrome) {
    result.return_value = gin::StringToV8(isolate, kChrome);
  } else {
    std::string domain_and_registry =
        net::registry_controlled_domains::GetDomainAndRegistry(
            url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    if (domain_and_registry.empty()) {
      result.return_value = gin::StringToV8(isolate, url.host());
    } else {
      domain_and_registry[0] = base::UTF16ToUTF8(base::i18n::ToUpper(
          base::UTF8ToUTF16(domain_and_registry.substr(0, 1))))[0];
      std::string_view domain(domain_and_registry);

      result.return_value =
          gin::StringToV8(isolate, domain.substr(0, domain.find_first_of('.')));
    }
  }
  return result;
}

RequestResult VivaldiUtilitiesHookDelegate::HandleGetVersion(
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value> &arguments) {
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
  std::string_view scheme = url.scheme_piece();
  if (URLPattern::IsValidSchemeForExtensions(scheme))
    return true;
  static const std::string_view extra_schemes[] = {
      url::kJavaScriptScheme,     url::kDataScheme,      url::kMailToScheme,
      content::kViewSourceScheme, kDevToolsLegacyScheme, kDevToolsScheme,
  };
  for (std::string_view extra_scheme : extra_schemes) {
    if (scheme == extra_scheme)
      return true;
  }

  // For about: urls only the blank page is supported.
  return url.IsAboutBlank();
}

}  // namespace
RequestResult VivaldiUtilitiesHookDelegate::HandleIsUrlValid(
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value> &arguments) {
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

RequestResult VivaldiUtilitiesHookDelegate::HandleIsRTL(
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>& arguments) {
  bool isRTL = base::i18n::IsRTL();

  v8::Isolate* isolate = context->GetIsolate();

  RequestResult result(RequestResult::HANDLED);
  result.return_value = v8::Boolean::New(isolate, isRTL);
  return result;
}
}  // namespace extensions

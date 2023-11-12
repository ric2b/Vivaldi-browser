// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ad_blocker/adblock_content_injection_handler.h"

#import <vector>

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/containers/adapters.h"
#import "base/json/json_string_value_serializer.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/string_util.h"
#import "base/time/time.h"
#import "components/ad_blocker/parse_utils.h"
#import "ios/ad_blocker/utils.h"
#import "ios/web/js_messaging/scoped_wk_script_message_handler.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider_observer.h"

namespace adblock_filter {

namespace {

constexpr char kMessageNamePrefix[] = "vivaldi_adblock_scriptlet_";
constexpr size_t kMessageNamePrefixLength =
    base::StringPiece(kMessageNamePrefix).length();

constexpr char kJSScriptArgRequestPart1[] =
    "window.webkit.messageHandlers['";
constexpr char kJSScriptArgRequestPart2[] = R"JsSource('].postMessage({}).then((scriptlet_arguments) =>  {
  const source=)JsSource";
constexpr char kJSScriptArgRequestPart3[] = R"JsSource(;
  if(scriptlet_arguments.length != 0) {
    new Function(source)();
  }
});)JsSource";

std::string SourceToTemplatedHexString(base::StringPiece source) {
  std::string result("`");
  // 200 should be able to hold the expansion of 9 placeholders.
  result.reserve(source.length() * 4 + 200);

  const int kPlaceHolderLength = 5;

  for (const auto* c = source.begin(); c != source.end(); ++c) {
    if (*c == '{' && source.end() - c >= kPlaceHolderLength) {
      if (*(c + 1) == '{' && base::IsAsciiDigit(*(c + 2)) && *(c + 3) == '}' &&
          *(c + 4) == '}') {
        int index = -1;
        base::StringToInt(base::StringPiece(c + 2, 1), &index);
        result.append("${scriptlet_arguments[");
        result.append(base::NumberToString(index - 1));
        result.append("]}");
        c += 5;
      }
    }

    result.append("\\x");
    result.append(base::HexEncode(c, 1));
  }

  result.push_back('`');
  return result;
}

class ContentInjectionHandlerImpl
    : public ContentInjectionHandler,
      public Resources::Observer,
      public web::WKWebViewConfigurationProviderObserver {
 public:
  explicit ContentInjectionHandlerImpl(web::BrowserState* browser_state,
                                       Resources* resources);
  ~ContentInjectionHandlerImpl() override;
  ContentInjectionHandlerImpl(const ContentInjectionHandlerImpl&) = delete;
  ContentInjectionHandlerImpl& operator=(const ContentInjectionHandlerImpl&) =
      delete;

  // Implementing ContentInjectionHandler
  void SetScriptletInjectionRules(RuleGroup group,
                                  base::Value::Dict injection_rules) override;

  // Implementing Resources::Observer
  void OnResourcesLoaded() override;

 private:
  // Implementing WKWebViewConfigurationProviderObserver
  void DidCreateNewConfiguration(
      web::WKWebViewConfigurationProvider* config_provider,
      WKWebViewConfiguration* new_config) override;

  void InjectUserScripts();
  void HandlePlaceholderRequest(WKScriptMessage* message,
                                ScriptMessageReplyHandler reply_handler);

  web::BrowserState* browser_state_;
  std::array<absl::optional<base::Value::Dict>, kRuleGroupCount>
      injection_rules_;

  Resources* resources_;

  __weak WKUserContentController* user_content_controller_;

  // NOTE(julien): Unclear why, but I am not managing to use a straight vector
  // of ScopedWKScriptMessageHandler and insert with emplace_back.
  // Doing so leads to a compilation error that I can't figure out.
  std::vector<std::unique_ptr<ScopedWKScriptMessageHandler>>
      script_message_handlers_;

  base::WeakPtrFactory<ContentInjectionHandlerImpl> weak_ptr_factory_{this};
};

ContentInjectionHandlerImpl::ContentInjectionHandlerImpl(
    web::BrowserState* browser_state,
    Resources* resources)
    : browser_state_(browser_state), resources_(resources) {
  web::WKWebViewConfigurationProvider& config_provider =
      web::WKWebViewConfigurationProvider::FromBrowserState(browser_state_);
  DidCreateNewConfiguration(&config_provider,
                            config_provider.GetWebViewConfiguration());
  config_provider.AddObserver(this);

  if (resources_->loaded())
    InjectUserScripts();
  else
    resources->AddObserver(this);
}

ContentInjectionHandlerImpl::~ContentInjectionHandlerImpl() {
  web::WKWebViewConfigurationProvider::FromBrowserState(browser_state_)
      .RemoveObserver(this);
}

void ContentInjectionHandlerImpl::OnResourcesLoaded() {
  resources_->RemoveObserver(this);
  InjectUserScripts();
}

void ContentInjectionHandlerImpl::DidCreateNewConfiguration(
    web::WKWebViewConfigurationProvider* config_provider,
    WKWebViewConfiguration* new_config) {
  user_content_controller_ = new_config.userContentController;

  if (resources_->loaded())
    InjectUserScripts();
}

void ContentInjectionHandlerImpl::SetScriptletInjectionRules(
    RuleGroup group,
    base::Value::Dict injection_rules) {
  injection_rules_[static_cast<size_t>(group)] = std::move(injection_rules);
}

void ContentInjectionHandlerImpl::InjectUserScripts() {
  std::map<std::string, base::StringPiece> injections =
      resources_->GetInjections();

  WKContentWorld* content_world =
      [WKContentWorld worldWithName:@"vivaldi_adblock_user_scripts"];

  for (const auto& [name, source] : injections) {
    std::string message_name(kMessageNamePrefix);
    message_name.append(name);

    std::string patched_source(kJSScriptArgRequestPart1);
    patched_source.append(message_name);
    patched_source.append(kJSScriptArgRequestPart2);
    patched_source.append(SourceToTemplatedHexString(source));
    patched_source.append(kJSScriptArgRequestPart3);

    script_message_handlers_.emplace_back(
        std::make_unique<ScopedWKScriptMessageHandler>(
            user_content_controller_,
            [NSString stringWithUTF8String:message_name.c_str()], content_world,
            base::BindRepeating(
                &ContentInjectionHandlerImpl::HandlePlaceholderRequest,
                weak_ptr_factory_.GetWeakPtr())));
    WKUserScript* user_script = [[WKUserScript alloc]
          initWithSource:[NSString stringWithUTF8String:patched_source.c_str()]
           injectionTime:WKUserScriptInjectionTimeAtDocumentStart
        forMainFrameOnly:false
          inContentWorld:content_world];
    [user_content_controller_ addUserScript:user_script];
  }
}

void ContentInjectionHandlerImpl::HandlePlaceholderRequest(
    WKScriptMessage* message,
    ScriptMessageReplyHandler reply_handler) {
  base::Value result(base::Value::Type::LIST);
  base::Value::List& result_list = result.GetList();

  std::string host([message.frameInfo.securityOrigin.host
      cStringUsingEncoding:NSUTF8StringEncoding]);

  std::string message_name(
      [message.name cStringUsingEncoding:NSUTF8StringEncoding]);
  CHECK(base::StartsWith(message_name, kMessageNamePrefix));
  std::string scriptlet_name = message_name.substr(kMessageNamePrefixLength);

  // We don't support other scriptlets yet.
  if (scriptlet_name != kAbpSnippetsScriptletName) {
    reply_handler(&result, nil);
    return;
  }

  std::string abp_argument = "[";
  const auto domain_pieces = base::SplitStringPiece(
      host, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    if (!injection_rules_[static_cast<size_t>(group)]) {
      continue;
    }

    std::set<const base::Value::List*, ContentInjectionArgumentsCompare>
        selected_arguments_lists;

    base::Value::Dict* subdomain_dict =
        &injection_rules_[static_cast<size_t>(group)].value();
    for (const auto& domain_piece: base::Reversed(domain_pieces)) {
      subdomain_dict = subdomain_dict->FindDict(domain_piece);
      if (!subdomain_dict) {
        break;
      }
      if (const auto* included =
              subdomain_dict->FindDict(rules_json::kIncluded)) {
        if (const auto* argument_list_list =
                included->FindList(scriptlet_name)) {
          for (const auto& argument_list : *argument_list_list) {
            DCHECK(argument_list.is_list());
            selected_arguments_lists.insert(&argument_list.GetList());
          }
        }
      }

      if (const auto* excluded =
              subdomain_dict->FindDict(rules_json::kExcluded)) {
        if (const auto* argument_list_list =
                excluded->FindList(scriptlet_name)) {
          for (const auto& argument_list : *argument_list_list) {
            DCHECK(argument_list.is_list());
            selected_arguments_lists.erase(&argument_list.GetList());
          }
        }
      }
    }

    for (const base::Value::List* selected_arguments_list :
         selected_arguments_lists) {
      DCHECK_EQ(1UL, selected_arguments_list->size());
      abp_argument.append(selected_arguments_list->front().GetString());
      abp_argument.push_back(',');
    }
  }

  abp_argument.pop_back();
  if (!abp_argument.empty()) {
    abp_argument.push_back(']');
    result_list.Append(abp_argument);
  }
  reply_handler(&result, nil);
}
}  // namespace

ContentInjectionHandler::~ContentInjectionHandler() = default;
/*static*/
std::unique_ptr<ContentInjectionHandler> ContentInjectionHandler::Create(
    web::BrowserState* browser_state,
    Resources* resources) {
  return std::make_unique<ContentInjectionHandlerImpl>(browser_state,
                                                       resources);
}

}  // namespace adblock_filter

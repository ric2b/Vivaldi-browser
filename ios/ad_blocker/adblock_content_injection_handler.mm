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

namespace adblock_filter {

namespace {

constexpr char kMessageNamePrefix[] = "vivaldi_adblock_scriptlet_";
constexpr size_t kMessageNamePrefixLength =
    std::string_view(kMessageNamePrefix).length();

constexpr char kJSScriptArgRequestPart1[] = "window.webkit.messageHandlers['";
constexpr char kJSScriptArgRequestPart2[] =
    R"JsSource('].postMessage({}).then((scriptlet_arguments) => {
  const source=)JsSource";
constexpr char kJSScriptArgRequestPart3[] = R"JsSource(;
  if(scriptlet_arguments.length != 0) {
    new Function(source)();
  }
});)JsSource";

std::string SourceToTemplatedHexString(std::string_view source) {
  std::string result("`");
  // 200 should be able to hold the expansion of 9 placeholders.
  result.reserve(source.length() * 4 + 200);

  const int kPlaceHolderLength = 5;

  for (auto c = source.begin(); c != source.end(); ++c) {
    if (*c == '{' && source.end() - c >= kPlaceHolderLength) {
      if (*(c + 1) == '{' && base::IsAsciiDigit(*(c + 2)) && *(c + 3) == '}' &&
          *(c + 4) == '}') {
        int index = -1;
        base::StringToInt(std::string_view(c + 2, c + 3), &index);
        result.append("${scriptlet_arguments[");
        result.append(base::NumberToString(index - 1));
        result.append("]}");
        c += 5;
      }
    }

    result.append("\\x");
    result.append(base::HexEncode(std::string_view(c, c + 1)));
  }

  result.push_back('`');
  return result;
}

class ContentInjectionHandlerImpl
    : public ContentInjectionHandler,
      public Resources::Observer {

 public:
  explicit ContentInjectionHandlerImpl(web::BrowserState* browser_state,
                                       Resources* resources);
  ~ContentInjectionHandlerImpl() override;
  ContentInjectionHandlerImpl(const ContentInjectionHandlerImpl&) = delete;
  ContentInjectionHandlerImpl& operator=(const ContentInjectionHandlerImpl&) =
      delete;

  // Implementing ContentInjectionHandler
  void SetIncognitoBrowserState(web::BrowserState* browser_state) override;
  void SetScriptletInjectionRules(RuleGroup group,
                                  base::Value::Dict injection_rules) override;

  // Implementing Resources::Observer
  void OnResourcesLoaded() override;

 private:
  void OnNewConfigurationCreated(
      web::WKWebViewConfigurationProvider* config_provider,
      WKWebViewConfiguration* new_config);
  void InjectUserScripts();
  void InjectUserScriptsForController(
      __weak WKUserContentController* user_content_controller);
  void HandlePlaceholderRequest(WKScriptMessage* message,
                                ScriptMessageReplyHandler reply_handler);

  base::WeakPtr<web::WKWebViewConfigurationProvider> config_provider_;
  base::WeakPtr<web::WKWebViewConfigurationProvider> incognito_config_provider_;
  std::array<std::optional<base::Value::Dict>, kRuleGroupCount>
      injection_rules_;

  Resources* resources_;

  __weak WKUserContentController* user_content_controller_;
  __weak WKUserContentController* incognito_user_content_controller_;

  // CallbackListSubscription members
  base::CallbackListSubscription main_config_subscription_;
  base::CallbackListSubscription incognito_config_subscription_;

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
    : config_provider_(
          web::WKWebViewConfigurationProvider::FromBrowserState(browser_state)
              .AsWeakPtr()),
      resources_(resources) {

  // Register callback for configuration changes in the main profile
  if (config_provider_) {
    // Apply for the existing configuration
    OnNewConfigurationCreated(
         config_provider_.get(), config_provider_->GetWebViewConfiguration());

    main_config_subscription_ =
        config_provider_->RegisterConfigurationCreatedCallback(
            base::BindRepeating(
               &ContentInjectionHandlerImpl::OnNewConfigurationCreated,
                   weak_ptr_factory_.GetWeakPtr(), config_provider_.get()));
  }

  if (!resources_->loaded())
    resources->AddObserver(this);
}

void ContentInjectionHandlerImpl::SetIncognitoBrowserState(
    web::BrowserState* browser_state) {
  if (incognito_config_provider_) {
    incognito_config_subscription_ = base::CallbackListSubscription();
    incognito_config_provider_.reset();
    incognito_user_content_controller_ = nullptr;
  }

  if (!browser_state) {
    return;
  }

  incognito_config_provider_ =
      web::WKWebViewConfigurationProvider::FromBrowserState(browser_state)
          .AsWeakPtr();
  if (incognito_config_provider_) {
    // Apply for the existing configuration
    OnNewConfigurationCreated(
        incognito_config_provider_.get(),
        incognito_config_provider_->GetWebViewConfiguration());

    incognito_config_subscription_ =
        incognito_config_provider_->RegisterConfigurationCreatedCallback(
            base::BindRepeating(
               &ContentInjectionHandlerImpl::OnNewConfigurationCreated,
                   weak_ptr_factory_.GetWeakPtr(),
                   incognito_config_provider_.get()));
  }
}

ContentInjectionHandlerImpl::~ContentInjectionHandlerImpl() {
  // No need to manually remove observers as
  // subscriptions are handled by CallbackListSubscription.
}

void ContentInjectionHandlerImpl::OnResourcesLoaded() {
  resources_->RemoveObserver(this);
  InjectUserScripts();
}

void ContentInjectionHandlerImpl::OnNewConfigurationCreated(
    web::WKWebViewConfigurationProvider* config_provider,
    WKWebViewConfiguration* new_config) {
  if (config_provider == config_provider_.get()) {
    user_content_controller_ = new_config.userContentController;
    if (resources_->loaded())
      InjectUserScriptsForController(user_content_controller_);
  } else if (config_provider == incognito_config_provider_.get()) {
    incognito_user_content_controller_ = new_config.userContentController;
    if (resources_->loaded())
      InjectUserScriptsForController(incognito_user_content_controller_);
  }
}

void ContentInjectionHandlerImpl::SetScriptletInjectionRules(
    RuleGroup group,
    base::Value::Dict injection_rules) {
  injection_rules_[static_cast<size_t>(group)] = std::move(injection_rules);
}

void ContentInjectionHandlerImpl::InjectUserScripts() {
  if (user_content_controller_)
    InjectUserScriptsForController(user_content_controller_);
  if (incognito_user_content_controller_)
    InjectUserScriptsForController(incognito_user_content_controller_);
}

void ContentInjectionHandlerImpl::InjectUserScriptsForController(
    __weak WKUserContentController* user_content_controller) {
  std::map<std::string, Resources::InjectableResource> injections =
      resources_->GetInjections();

  WKContentWorld* content_world =
      [WKContentWorld worldWithName:@"vivaldi_adblock_user_scripts"];

  for (const auto& [name, injectable_resource] : injections) {
    std::string message_name(kMessageNamePrefix);
    message_name.append(name);

    std::string patched_source(kJSScriptArgRequestPart1);
    patched_source.append(message_name);
    patched_source.append(kJSScriptArgRequestPart2);
    patched_source.append(SourceToTemplatedHexString(injectable_resource.code));
    patched_source.append(kJSScriptArgRequestPart3);

    script_message_handlers_.emplace_back(
        std::make_unique<ScopedWKScriptMessageHandler>(
            user_content_controller,
            [NSString stringWithUTF8String:message_name.c_str()], content_world,
            base::BindRepeating(
                &ContentInjectionHandlerImpl::HandlePlaceholderRequest,
                weak_ptr_factory_.GetWeakPtr())));
    WKUserScript* user_script = [[WKUserScript alloc]
          initWithSource:[NSString stringWithUTF8String:patched_source.c_str()]
           injectionTime:WKUserScriptInjectionTimeAtDocumentStart
        forMainFrameOnly:false
          inContentWorld:injectable_resource.use_main_world
                             ? WKContentWorld.pageWorld
                             : content_world];
    [user_content_controller addUserScript:user_script];
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
  if (scriptlet_name != kAbpSnippetsMainScriptletName &&
      scriptlet_name != kAbpSnippetsIsolatedScriptletName) {
    reply_handler(&result, nil);
    return;
  }

  std::string abp_argument;
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
    for (const auto& domain_piece : base::Reversed(domain_pieces)) {
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
      // The ABP snippet arguments were purposefully left with a trailing
      // comma at the parsing stage. We can just concatenate them here.
      abp_argument.append(selected_arguments_list->front().GetString());
    }
  }

  if (!abp_argument.empty()) {
    // Remove extra comma
    abp_argument.pop_back();
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

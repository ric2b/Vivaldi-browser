// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_content_injection_rule.h"

#include <iomanip>
#include <ostream>

namespace adblock_filter {

namespace {
constexpr int kAlignemntPosition = 20;
constexpr int kAlignemntPositionNoColon = kAlignemntPosition - 1;

void PrintStrings(std::ostream& os, std::set<std::string> strings) {
  if (strings.empty()) {
    os << ":<NULL>\n";
    return;
  }

  std::string result;
  for (const auto& string : strings) {
    os << ':' << string << "\n" << std::string(kAlignemntPosition, ' ');
  }
  os.seekp(-kAlignemntPosition, std::ios_base::cur);
}
}  // namespace

ContentInjectionRuleCore::ContentInjectionRuleCore() = default;
ContentInjectionRuleCore::~ContentInjectionRuleCore() = default;
ContentInjectionRuleCore::ContentInjectionRuleCore(
    ContentInjectionRuleCore&& other) = default;
ContentInjectionRuleCore::ContentInjectionRuleCore(
    const ContentInjectionRuleCore& other) = default;
ContentInjectionRuleCore& ContentInjectionRuleCore::operator=(
    ContentInjectionRuleCore&& other) = default;
bool ContentInjectionRuleCore::operator==(
    const ContentInjectionRuleCore& other) const {
  return is_allow_rule == other.is_allow_rule &&
         excluded_domains == other.excluded_domains &&
         included_domains == other.included_domains;
}

ContentInjectionRuleCore ContentInjectionRuleCore::Clone() {
  return ContentInjectionRuleCore(*this);
}

std::ostream& operator<<(std::ostream& os,
                         const ContentInjectionRuleCore& rule) {
  os << "\n"
     << std::setw(kAlignemntPosition) << "Allow rule:" << rule.is_allow_rule
     << "\n"
     << std::setw(kAlignemntPositionNoColon) << "Included domains";
  PrintStrings(os, rule.included_domains);
  os << std::setw(kAlignemntPositionNoColon) << "Excluded domains";
  PrintStrings(os, rule.excluded_domains);

  return os;
}

CosmeticRule::CosmeticRule() = default;
CosmeticRule::~CosmeticRule() = default;
CosmeticRule::CosmeticRule(CosmeticRule&& other) = default;
CosmeticRule& CosmeticRule::operator=(CosmeticRule&& other) = default;

bool CosmeticRule::operator==(const CosmeticRule& other) const {
  return core == other.core && selector == other.selector;
}

std::ostream& operator<<(std::ostream& os, const CosmeticRule& rule) {
  return os << "\n"
            << std::setw(kAlignemntPosition) << "Selector:" << rule.selector
            << rule.core;
}

ScriptletInjectionRule::ScriptletInjectionRule() = default;
ScriptletInjectionRule::~ScriptletInjectionRule() = default;
ScriptletInjectionRule::ScriptletInjectionRule(ScriptletInjectionRule&& other) =
    default;
ScriptletInjectionRule& ScriptletInjectionRule::operator=(
    ScriptletInjectionRule&& other) = default;
bool ScriptletInjectionRule::operator==(
    const ScriptletInjectionRule& other) const {
  return scriptlet_name == other.scriptlet_name &&
         arguments == other.arguments && core == other.core;
}

std::ostream& operator<<(std::ostream& os, const ScriptletInjectionRule& rule) {
  os << "\n"
     << std::setw(kAlignemntPosition) << "Scriptlet:" << rule.scriptlet_name
     << "\n"
     << std::setw(kAlignemntPosition) << "Arguments:";

  for (const auto& argument : rule.arguments)
    os << argument << " ";
  return os << rule.core;
}

}  // namespace adblock_filter

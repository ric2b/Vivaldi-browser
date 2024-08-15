// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_content_injection_rule.h"

#include <iomanip>

namespace adblock_filter {

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
  os << std::endl
     << std::setw(20) << "Allow rule:" << rule.is_allow_rule << std::endl
     << std::setw(20) << "Included domains:";
  for (const auto& included_domain : rule.included_domains)
    os << included_domain << "|";
  os << std::endl << std::setw(20) << "Excluded domains:";
  for (const auto& excluded_domain : rule.excluded_domains)
    os << excluded_domain << "|";
  return os << std::endl;
}

CosmeticRule::CosmeticRule() = default;
CosmeticRule::~CosmeticRule() = default;
CosmeticRule::CosmeticRule(CosmeticRule&& other) = default;
CosmeticRule& CosmeticRule::operator=(CosmeticRule&& other) = default;

bool CosmeticRule::operator==(const CosmeticRule& other) const {
  return core == other.core && selector == other.selector;
}

std::ostream& operator<<(std::ostream& os, const CosmeticRule& rule) {
  return os << std::endl << std::setw(20) << rule.selector << rule.core;
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
  os << std::endl << std::setw(20) << rule.scriptlet_name << std::endl;
  for (const auto& argument : rule.arguments)
    os << std::setw(30) << argument << std::endl;
  return os << rule.core;
}

}  // namespace adblock_filter

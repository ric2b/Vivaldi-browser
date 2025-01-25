// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_CONTENT_INJECTION_RULE_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_CONTENT_INJECTION_RULE_H_

#include <set>
#include <string>
#include <vector>

namespace adblock_filter {

struct ContentInjectionRuleCore {
 public:
  ContentInjectionRuleCore();
  ~ContentInjectionRuleCore();
  ContentInjectionRuleCore(ContentInjectionRuleCore&& other);
  ContentInjectionRuleCore& operator=(ContentInjectionRuleCore&& other);
  bool operator==(const ContentInjectionRuleCore& other) const;

  ContentInjectionRuleCore Clone();

  bool is_allow_rule = false;

  std::set<std::string> included_domains;
  std::set<std::string> excluded_domains;

 private:
  ContentInjectionRuleCore(const ContentInjectionRuleCore& other);
};

struct CosmeticRule {
 public:
  CosmeticRule();
  ~CosmeticRule();
  CosmeticRule(CosmeticRule&& other);
  CosmeticRule& operator=(CosmeticRule&& other);
  bool operator==(const CosmeticRule& other) const;

  ContentInjectionRuleCore core;

  std::string selector;
};

using CosmeticRules = std::vector<CosmeticRule>;

struct ScriptletInjectionRule {
 public:
  ScriptletInjectionRule();
  ~ScriptletInjectionRule();
  ScriptletInjectionRule(ScriptletInjectionRule&& other);
  ScriptletInjectionRule& operator=(ScriptletInjectionRule&& other);
  bool operator==(const ScriptletInjectionRule& other) const;

  ContentInjectionRuleCore core;

  std::string scriptlet_name;
  std::vector<std::string> arguments;
};

using ScriptletInjectionRules = std::vector<ScriptletInjectionRule>;

// Used for unit tests.
std::ostream& operator<<(std::ostream& os,
                         const ContentInjectionRuleCore& rule);
std::ostream& operator<<(std::ostream& os, const CosmeticRule& rule);
std::ostream& operator<<(std::ostream& os, const ScriptletInjectionRule& rule);
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_CONTENT_INJECTION_RULE_H_

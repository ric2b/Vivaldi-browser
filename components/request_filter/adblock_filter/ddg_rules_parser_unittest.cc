// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include <ostream>

#include "base/json/json_string_value_serializer.h"
#include "components/request_filter/adblock_filter/adblock_filter_rule.h"
#include "components/request_filter/adblock_filter/ddg_rules_parser.h"
#include "components/request_filter/adblock_filter/parse_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace adblock_filter {

namespace {
std::unique_ptr<base::Value> ParseJSON(const std::string& json) {
  JSONStringValueDeserializer serializer(json);
  std::string json_error;
  std::unique_ptr<base::Value> root(
      serializer.Deserialize(nullptr, &json_error));
  DCHECK_NE(nullptr, root.get()) << json << "\n" << json_error;

  return root;
}
}  // namespace

TEST(DuckDuckGoRulesParserTest, NothingParsed) {
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);

  EXPECT_EQ(AdBlockMetadata(), parse_result.metadata);

  EXPECT_EQ(0u, parse_result.filter_rules.size());
}

TEST(DuckDuckGoRulesParserTest, SimpleBlock) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "badsite.com": {
        "default": "block"
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "badsite.com";
  expected_rules.back().host = "badsite.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(FilterRule::kAnchorHost);
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, SimpleIgnore) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "gooddsite.com": {
        "default": "ignore"
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, SimpleRuleBlock) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "mostly_good.com": {
        "default": "ignore",
        "rules": [
          {
            "rule": "mostly_good\\.com\\/with\\/a\\/tracker"
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "mostly_good.com/with/a/tracker";
  expected_rules.back().host = "mostly_good.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, SimpleRuleAllow) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "mostly_bad.com": {
        "default": "block",
        "rules": [
          {
            "rule": "mostly_bad\\.com\\/except\\/for\\/this",
            "action": "ignore"
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "mostly_bad.com";
  expected_rules.back().host = "mostly_bad.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(FilterRule::kAnchorHost);
  expected_rules.back().pattern_type = FilterRule::kPlain;

  expected_rules.emplace_back();
  expected_rules.back().is_allow_rule = true;
  expected_rules.back().pattern = "mostly_bad.com/except/for/this";
  expected_rules.back().host = "mostly_bad.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleBlockWithOptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "default": "ignore",
        "rules": [
          {
            "rule": "example\\.com\\/bad\\/",
            "options": {
              "domains": [ "bad_with_example.com" ],
              "types": [ "script" ]
            }
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "example.com/bad/";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set(FilterRule::kScript);
  expected_rules.back().party.set();
  expected_rules.back().included_domains.push_back("bad_with_example.com");
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleAllowWithOptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/required\\/on\\/first\\/party\\/",
            "action": "ignore",
            "options": {
              "domains": [ "bad_site.com" ],
              "types": [ "object" ]
            }
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(FilterRule::kAnchorHost);
  expected_rules.back().pattern_type = FilterRule::kPlain;

  expected_rules.emplace_back();
  expected_rules.back().is_allow_rule = true;
  expected_rules.back().pattern = "bad_site.com/required/on/first/party/";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set(FilterRule::kObject);
  expected_rules.back().party.set();
  expected_rules.back().included_domains.push_back("bad_site.com");
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleBlockWithExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "default": "ignore",
        "rules": [
          {
            "rule": "example\\.com\\/bad\\/",
            "exceptions": {
              "domains": [ "good_with_example.com" ],
              "types": [ "image" ]
            }
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "example.com/bad/";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().resource_types.reset(FilterRule::kImage);
  expected_rules.back().party.set();
  expected_rules.back().excluded_domains.push_back("good_with_example.com");
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleAllowWithExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/with\\/this\\/mostly\\/good\\/resource",
            "action": "ignore",
            "exceptions": {
              "domains": [ "always_bad.com" ],
              "types": [ "stylesheet" ]
            }
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(FilterRule::kAnchorHost);
  expected_rules.back().pattern_type = FilterRule::kPlain;

  expected_rules.emplace_back();
  expected_rules.back().is_allow_rule = true;
  expected_rules.back().pattern = "bad_site.com/with/this/mostly/good/resource";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().resource_types.reset(FilterRule::kStylesheet);
  expected_rules.back().party.set();
  expected_rules.back().excluded_domains.push_back("always_bad.com");
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleBlockWithOptionsAndExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "default": "ignore",
        "rules": [
          {
            "rule": "example\\.com\\/bad\\/",
            "options": {
              "domains": [ "bad.with_example.com" ]
            },
            "exceptions": {
              "domains": [ "not.bad.with_example.com" ]
            }
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "example.com/bad/";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().included_domains.push_back("bad.with_example.com");
  expected_rules.back().excluded_domains.push_back("not.bad.with_example.com");
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleAllowWithOptionsAndExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "default": "block",
        "rules": [
          {
            "rule": "example\\.com\\/except\\/this",
            "action": "ignore",
            "options": {
              "types": [ "media" ]
            },
            "exceptions": {
              "types": [ "stylesheet" ]
            }
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "example.com";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(FilterRule::kAnchorHost);
  expected_rules.back().pattern_type = FilterRule::kPlain;

  expected_rules.emplace_back();
  expected_rules.back().is_allow_rule = true;
  expected_rules.back().pattern = "example.com/except/this";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set(FilterRule::kMedia);
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleBlockFromExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "default": "ignore",
        "rules": [
          {
            "rule": "example\\.com\\/usually_good\\/",
            "action": "ignore",
            "exceptions": {
              "domains": [ "bad.with_example.com" ],
              "types": [ "script" ]
            }
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "example.com/usually_good/";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set(FilterRule::kScript);
  expected_rules.back().party.set();
  expected_rules.back().included_domains.push_back("bad.with_example.com");
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleAllowFromExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/but\\/these\\/images\\/",
            "exceptions": {
              "types": [ "image" ]
            }
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(FilterRule::kAnchorHost);
  expected_rules.back().pattern_type = FilterRule::kPlain;

  expected_rules.emplace_back();
  expected_rules.back().is_allow_rule = true;
  expected_rules.back().pattern = "bad_site.com/but/these/images/";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set(FilterRule::kImage);
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = FilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RegexRuleBlock) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "mostly_good.com": {
        "default": "ignore",
        "rules": [
          {
            "rule": "mostly_good\\.com\\/with\\/(a|another)\\/tracker"
          }
        ]
      }
    },
    "entities" : {}
  })JSON");

  FilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().ngram_search_string = "mostly_good.com/with/*/tracker";
  expected_rules.back().pattern =
      "mostly_good\\.com\\/with\\/(a|another)\\/tracker";
  expected_rules.back().host = "mostly_good.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = FilterRule::kRegex;

  ASSERT_EQ(expected_rules.size(), parse_result.filter_rules.size());

  auto actual_rules_it = parse_result.filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

}  // namespace adblock_filter
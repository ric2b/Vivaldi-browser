// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/ddg_rules_parser.h"

#include <ostream>

#include "base/json/json_string_value_serializer.h"
#include "components/ad_blocker/adblock_request_filter_rule.h"
#include "components/ad_blocker/parse_result.h"
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

  EXPECT_EQ(0u, parse_result.request_filter_rules.size());
}

TEST(DuckDuckGoRulesParserTest, SimpleBlock) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "badsite.com": {
        "owner": {
          "name" : "Bad Site Inc."
        },
        "default": "block"
      }
    },
    "entities" : {
      "Bad Site Inc.": {
        "domains": [
          "badsite.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "badsite.com";
  expected_rules.back().host = "badsite.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("badsite.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, SimpleIgnore) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "goodsite.com": {
        "owner": {
          "name" : "Good Site Inc."
        },
        "default": "ignore"
      }
    },
    "entities" : {
      "Good Site Inc.": {
        "domains": [
          "goodsite.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}
TEST(DuckDuckGoRulesParserTest, SimpleRuleBlock) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "mostly_good.com": {
        "owner": {
          "name" : "Mostly Good Site Inc."
        },
        "default": "ignore",
        "rules": [
          {
            "rule": "mostly_good\\.com\\/with\\/a\\/tracker"
          }
        ]
      }
    },
    "entities" : {
      "Mostly Good Site Inc.": {
        "domains": [
          "mostly_good.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "mostly_good.com/with/a/tracker";
  expected_rules.back().host = "mostly_good.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("mostly_good.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, SimpleRuleAllow) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "mostly_bad.com": {
        "owner": {
          "name" : "Mostly Bad Site Inc."
        },
        "default": "block",
        "rules": [
          {
            "rule": "mostly_bad\\.com\\/except\\/for\\/this",
            "action": "ignore"
          }
        ]
      }
    },
    "entities" : {
      "Mostly Bad Site Inc.": {
        "domains": [
          "mostly_bad.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "mostly_bad.com";
  expected_rules.back().host = "mostly_bad.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("mostly_bad.com");

  expected_rules.emplace_back();
  expected_rules.back().decision = RequestFilterRule::kPass;
  expected_rules.back().pattern = "mostly_bad.com/except/for/this";
  expected_rules.back().host = "mostly_bad.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("mostly_bad.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleBlockWithOptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "owner": {
          "name" : "The Example Company"
        },
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
    "entities" : {
      "The Example Company": {
        "domains": [
          "example.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "example.com/bad/";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set(RequestFilterRule::kScript);
  expected_rules.back().party.set();
  expected_rules.back().included_domains.insert("bad_with_example.com");
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("example.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleAllowWithOptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "owner": {
          "name" : "Bad Site Inc."
        },
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/required\\/on\\/first\\/party\\/",
            "action": "ignore",
            "options": {
              "domains": [ "needs_bad_site.com" ],
              "types": [ "object" ]
            }
          }
        ]
      }
    },
    "entities" : {
      "Bad Site Inc.": {
        "domains": [
          "bad_site.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  expected_rules.emplace_back();
  expected_rules.back().decision = RequestFilterRule::kPass;
  expected_rules.back().pattern = "bad_site.com/required/on/first/party/";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set(RequestFilterRule::kObject);
  expected_rules.back().party.set();
  expected_rules.back().included_domains.insert("needs_bad_site.com");
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleBlockWithExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "owner": {
          "name" : "The Example Company"
        },
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
    "entities" : {
      "The Example Company": {
        "domains": [
          "example.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "example.com/bad/";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().resource_types.reset(RequestFilterRule::kImage);
  expected_rules.back().party.set();
  expected_rules.back().excluded_domains.insert("good_with_example.com");
  expected_rules.back().excluded_domains.insert("example.com");
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleAllowWithExceptions) {
  // Expecting exceptions to be ignored to match DDG implementation.
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "owner": {
          "name" : "Bad Site Inc."
        },
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/with\\/this\\/mostly\\/good\\/resource",
            "action": "ignore",
            "exceptions": {
              "domains": [ "other_good_domain.com" ],
              "types": [ "stylesheet" ]
            }
          }
        ]
      }
    },
    "entities" : {
      "Bad Site Inc.": {
        "domains": [
          "bad_site.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  expected_rules.emplace_back();
  expected_rules.back().decision = RequestFilterRule::kPass;
  expected_rules.back().pattern = "bad_site.com/with/this/mostly/good/resource";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleBlockWithOptionsAndExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "owner": {
          "name" : "The Example Company"
        },
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
    "entities" : {
      "The Example Company": {
        "domains": [
          "example.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "example.com/bad/";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().included_domains.insert("bad.with_example.com");
  expected_rules.back().excluded_domains.insert("not.bad.with_example.com");
  expected_rules.back().excluded_domains.insert("example.com");
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleAllowWithOptionsAndExceptions) {
  // Expecting exceptions to be ignored to match DDG implementation.
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "owner": {
          "name" : "The Example Company"
        },
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
    "entities" : {
      "The Example Company": {
        "domains": [
          "example.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "example.com";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("example.com");

  expected_rules.emplace_back();
  expected_rules.back().decision = RequestFilterRule::kPass;
  expected_rules.back().pattern = "example.com/except/this";
  expected_rules.back().host = "example.com";
  expected_rules.back().resource_types.set(RequestFilterRule::kMedia);
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("example.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RedundantIgnore) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "example.com": {
        "owner": {
          "name" : "The Example Company"
        },
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
    "entities" : {
      "The Example Company": {
        "domains": [
          "example.com"
        ]
      }
    }
  })JSON");

  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);
  EXPECT_EQ(1, parse_result.rules_info.unsupported_rules);
  EXPECT_EQ(0UL, parse_result.request_filter_rules.size());
}

TEST(DuckDuckGoRulesParserTest, RedundantBlock) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "owner": {
          "name" : "Bad Site Inc."
        },
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/really_bad\\/"
          }
        ]
      }
    },
    "entities" : {
      "Bad Site Inc.": {
        "domains": [
          "bad_site.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  EXPECT_EQ(1, parse_result.rules_info.unsupported_rules);
  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleAllowFromExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "owner": {
          "name" : "Bad Site Inc."
        },
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
    "entities" : {
      "Bad Site Inc.": {
        "domains": [
          "bad_site.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  expected_rules.emplace_back();
  expected_rules.back().decision = RequestFilterRule::kPass;
  expected_rules.back().pattern = "bad_site.com/but/these/images/";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set(RequestFilterRule::kImage);
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RuleAllowFromOptionsAndExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "owner": {
          "name" : "Bad Site Inc."
        },
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/special\\/",
            "options": {
              "types": [ "script", "stylesheet" ],
              "domains": [
                "subdomain.example.com",
                "other_site.com",
                "trusted.com",
                "untrusted.com"
              ]
            },
            "exceptions": {
              "types": [ "image", "stylesheet" ],
              "domains": [
                "another.sub.other_site.com",
                "trusted.com",
                "sub.other_site.com",
                "example.com",
                "useless.com"
              ]
            }
          }
        ]
      }
    },
    "entities" : {
      "Bad Site Inc.": {
        "domains": [
          "bad_site.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  expected_rules.emplace_back();
  expected_rules.back().decision = RequestFilterRule::kPass;
  expected_rules.back().pattern = "bad_site.com/special/";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set(RequestFilterRule::kStylesheet);
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().included_domains.insert("subdomain.example.com");
  expected_rules.back().included_domains.insert("sub.other_site.com");
  expected_rules.back().included_domains.insert("trusted.com");
  expected_rules.back().excluded_domains.insert("bad_site.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, RegexRuleBlock) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "mostly_good.com": {
        "owner": {
          "name" : "Mostly Good Site Inc."
        },
        "default": "ignore",
        "rules": [
          {
            "rule": "mostly_good\\.com\\/with\\/(a|another)\\/tracker"
          }
        ]
      }
    },
    "entities" : {
      "Mostly Good Site Inc.": {
        "domains": [
          "mostly_good.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
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
  expected_rules.back().excluded_domains.insert("mostly_good.com");
  expected_rules.back().pattern_type = RequestFilterRule::kRegex;

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, SimpleSurrogate) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "owner": {
          "name" : "Bad Site Inc."
        },
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/bad_script\\.js",
            "surrogate": "bad_script.js"
          }
        ]
      }
    },
    "entities" : {
      "Bad Site Inc.": {
        "domains": [
          "bad_site.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com/bad_script.js";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().modifier = RequestFilterRule::kRedirect;
  expected_rules.back().modifier_values.insert("bad_script.js");
  expected_rules.back().excluded_domains.insert("bad_site.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, SurrogateWithOptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "owner": {
          "name" : "Bad Site Inc."
        },
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/bad_script\\.js",
            "surrogate": "bad_script.js",
            "options": {
              "domains": [
                "use_bad_script.com"
              ]
            }
          }
        ]
      }
    },
    "entities" : {
      "Bad Site Inc.": {
        "domains": [
          "bad_site.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com/bad_script.js";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().modifier = RequestFilterRule::kRedirect;
  expected_rules.back().modifier_values.insert("bad_script.js");
  expected_rules.back().included_domains.insert("use_bad_script.com");
  expected_rules.back().excluded_domains.insert("bad_site.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, SurrogateWithExceptions) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "bad_site.com": {
        "owner": {
          "name" : "Bad Site Inc."
        },
        "default": "block",
        "rules": [
          {
            "rule": "bad_site\\.com\\/bad_script\\.js",
            "surrogate": "bad_script.js",
            "exceptions": {
              "domains": [
                "allow_bad_script.com"
              ]
            }
          }
        ]
      }
    },
    "entities" : {
      "Bad Site Inc.": {
        "domains": [
          "bad_site.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("bad_site.com");

  expected_rules.emplace_back();
  expected_rules.back().decision = RequestFilterRule::kPass;
  expected_rules.back().pattern = "bad_site.com/bad_script.js";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().included_domains.insert("allow_bad_script.com");
  expected_rules.back().excluded_domains.insert("bad_site.com");

  expected_rules.emplace_back();
  expected_rules.back().pattern = "bad_site.com/bad_script.js";
  expected_rules.back().host = "bad_site.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().modifier = RequestFilterRule::kRedirect;
  expected_rules.back().modifier_values.insert("bad_script.js");
  expected_rules.back().excluded_domains.insert("bad_site.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, SimpleBlockWithSurrogate) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "mostly_good.com": {
        "owner": {
          "name" : "Mostly Good Site Inc."
        },
        "default": "ignore",
        "rules": [
          {
            "rule": "mostly_good\\.com\\/tracking\\.js",
            "surrogate": "tracking.js"
          }
        ]
      }
    },
    "entities" : {
      "Mostly Good Site Inc.": {
        "domains": [
          "mostly_good.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "mostly_good.com/tracking.js";
  expected_rules.back().host = "mostly_good.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().modifier = RequestFilterRule::kRedirect;
  expected_rules.back().modifier_values.insert("tracking.js");
  expected_rules.back().excluded_domains.insert("mostly_good.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

TEST(DuckDuckGoRulesParserTest, NoSurrogateWhenIgnore) {
  auto root = ParseJSON(R"JSON({
    "trackers" : {
      "mostly_bad.com": {
        "owner": {
          "name" : "Mostly Bad Site Inc."
        },
        "default": "block",
        "rules": [
          {
            "rule": "mostly_bad\\.com\\/except\\/for\\/this",
            "surrogate": "ignored_surrogate",
            "action": "ignore"
          }
        ]
      }
    },
    "entities" : {
      "Mostly Bad Site Inc.": {
        "domains": [
          "mostly_bad.com"
        ]
      }
    }
  })JSON");

  RequestFilterRules expected_rules;
  ParseResult parse_result;
  DuckDuckGoRulesParser parser(&parse_result);
  parser.Parse(*root);

  expected_rules.emplace_back();
  expected_rules.back().pattern = "mostly_bad.com";
  expected_rules.back().host = "mostly_bad.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().anchor_type.set(RequestFilterRule::kAnchorHost);
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("mostly_bad.com");

  expected_rules.emplace_back();
  expected_rules.back().decision = RequestFilterRule::kPass;
  expected_rules.back().pattern = "mostly_bad.com/except/for/this";
  expected_rules.back().host = "mostly_bad.com";
  expected_rules.back().resource_types.set();
  expected_rules.back().party.set();
  expected_rules.back().pattern_type = RequestFilterRule::kPlain;
  expected_rules.back().excluded_domains.insert("mostly_bad.com");

  ASSERT_EQ(expected_rules.size(), parse_result.request_filter_rules.size());

  auto actual_rules_it = parse_result.request_filter_rules.begin();
  for (const auto& rule : expected_rules) {
    EXPECT_EQ(rule, *actual_rules_it);
    actual_rules_it++;
  }
}

}  // namespace adblock_filter
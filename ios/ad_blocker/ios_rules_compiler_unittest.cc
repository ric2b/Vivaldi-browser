#include "ios/ad_blocker/ios_rules_compiler.h"

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "components/ad_blocker/adblock_rule_parser.h"
#include "components/ad_blocker/parse_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace adblock_filter {
namespace {
bool FormatJSON(std::string& json) {
  JSONStringValueDeserializer deserializer(json);
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  int error_code;
  std::string error;
  auto value = deserializer.Deserialize(&error_code, &error);
  if (!value) {
    LOG(ERROR) << error_code << ":" << error;
    return false;
  }
  serializer.Serialize(*value);
  json.swap(output);
  return true;
}
}  // namespace

TEST(AdBlockIosRuleCompilerTest, SimpleRule) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("example");
  rule_parser.Parse("blåbærsyltetøy");
  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "example",
              "url-filter-is-case-sensitive": false,
              "load-context": ["child-frame"],
              "resource-type": [
                "document"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "example",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "style-sheet",
                "image",
                "media",
                "script",
                "fetch",
                "font",
                "media",
                "websocket",
                "ping",
                "other"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      }
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, RuleWithResource) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("something$script");
  rule_parser.Parse("something$image,match-case");
  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "something",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "something",
              "url-filter-is-case-sensitive": true,
              "resource-type": [
                "image"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      }
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, SubdocumentRule) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("something$subdocument");
  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "something",
              "url-filter-is-case-sensitive": false,
              "load-context": [
                "child-frame"
              ],
              "resource-type": [
                "document"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      }
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, RuleWithParty) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("something_from_self$script,~third-party");
  rule_parser.Parse("something_from_others$script,third-party");
  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "something_from_self",
              "url-filter-is-case-sensitive": false,
              "load-type": [
                "first-party"
              ],
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "something_from_others",
              "url-filter-is-case-sensitive": false,
              "load-type": [
                "third-party"
              ],
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      }
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, AllowRuleWithResource) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("@@something$script");
  std::string expected(R"===({
    "network": {
      "allow": [
        {
          "trigger": {
            "url-filter": "something",
            "url-filter-is-case-sensitive": false,
            "resource-type": [
              "script"
            ]
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        }
      ]
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, AnchoredRules) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("|https://example.com/$script");
  rule_parser.Parse("||google.com/$script");
  rule_parser.Parse("ad.js|$script");

  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "^https:\\/\\/example\\.com\\/",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?google\\.com\\/",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "ad\\.js$",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      }
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, WildcardsAndSpecialCharsRules) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("part1*part2?part(3)$ping");
  rule_parser.Parse("example.com^bad^$websocket");
  rule_parser.Parse("google.com^|$media");
  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "part1.*part2\\?part\\(3\\)",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "ping"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "example\\.com[^a-zA-Z0-9_.%-]bad([^a-zA-Z0-9_.%-].*)?$",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "websocket"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "google\\.com[^a-zA-Z0-9_.%-]?$",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "media"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      }
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, RulesWithHost) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("advert$host=example.com,image");
  rule_parser.Parse("foo.com/bar$host=foo.com,image");
  rule_parser.Parse("google.com/something$host=evil.google.com,image");
  rule_parser.Parse("ads.example.com/something$host=example.com,image");
  rule_parser.Parse("baz$host=baz.com,image");
  rule_parser.Parse("xxx.elg.no$host=elg.no,image");
  rule_parser.Parse("ulv.no.zzz$host=ulv.no,image");
  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?example\\.com[^a-zA-Z0-9_.%-].*advert",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "image"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?foo\\.com\\/bar",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "image"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?evil\\.google\\.com\\/something",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "image"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?ads\\.example\\.com\\/something",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "image"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?baz\\.com[^a-zA-Z0-9_.%-]",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "image"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?xxx\\.elg\\.no[^a-zA-Z0-9_.%-]",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "image"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?ulv\\.no[^a-zA-Z0-9_.%-].*ulv\\.no\\.zzz",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "image"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      }
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, RegexRules) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("/ad(vert)?[0-9]$/$image");
  rule_parser.Parse("/ba{1-3}d/$image");
  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "ad(vert)?[0-9]$",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "image"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      }
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, DocumentActivationRules) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("evil$document");
  rule_parser.Parse("dangerous$script,document");
  rule_parser.Parse("@@good$image,document");
  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "evil",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "document"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "dangerous",
              "url-filter-is-case-sensitive": false,
              "resource-type": [
                "script",
                "document"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      },
      "allow" : [
        {
          "trigger": {
            "url-filter": "good",
            "url-filter-is-case-sensitive": false,
            "resource-type": [
              "image"
            ]
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        },
        {
          "trigger": {
            "url-filter": ".*",
            "url-filter-is-case-sensitive": false,
            "if-top-url": [ "good" ],
            "top-url-filter-is-case-sensitive": false
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        }
      ]
    },
    "cosmetic": {
      "allow": [
        {
          "trigger": {
            "url-filter": ".*",
            "url-filter-is-case-sensitive": false,
            "if-top-url": [ "good" ],
            "top-url-filter-is-case-sensitive": false
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        }
      ]
    },
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, OtherActivations) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("@@no.generic.blocks$genericblock");
  rule_parser.Parse("@@no.generic.hide$generichide,match-case");
  rule_parser.Parse("@@no.element.hide$elemhide");
  std::string expected(R"===({
    "network": {
      "generic-allow" : [
        {
          "trigger": {
            "url-filter": ".*",
            "url-filter-is-case-sensitive": false,
            "if-top-url": [ "no\\.generic\\.blocks" ],
            "top-url-filter-is-case-sensitive": false
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        }
      ]
    },
    "cosmetic": {
      "allow" : [
        {
          "trigger": {
            "url-filter": ".*",
            "url-filter-is-case-sensitive": false,
            "if-top-url": [ "no\\.element\\.hide" ],
            "top-url-filter-is-case-sensitive": false
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        }
      ],
      "generic-allow" : [
        {
          "trigger": {
            "url-filter": ".*",
            "url-filter-is-case-sensitive": false,
            "if-top-url": [ "no\\.generic\\.hide" ],
            "top-url-filter-is-case-sensitive": true
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        }
      ]
    },
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, RulesWithIncludedDomains) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("danger$domain=evil.com,script");
  rule_parser.Parse("@@allowed$domain=nice.com,script");
  std::string expected(R"===({
    "network": {
      "block": {
        "specific": [
          {
            "trigger": {
              "url-filter": "danger",
              "url-filter-is-case-sensitive": false,
              "if-top-url": [ "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?evil\\.com[:\\/]" ],
              "top-url-filter-is-case-sensitive": true,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      },
      "allow": [
        {
          "trigger": {
            "url-filter": "allowed",
            "url-filter-is-case-sensitive": false,
            "if-top-url": [ "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?nice\\.com[:\\/]" ],
            "top-url-filter-is-case-sensitive": true,
            "resource-type": [
              "script"
            ]
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        }
      ]
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, RulesWithExcludedDomains) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("danger$domain=~nice.com,script");
  rule_parser.Parse("@@allowed$domain=~evil.com,script");
  std::string expected(R"===({
    "network": {
      "block": {
        "generic": [
          {
            "trigger": {
              "url-filter": "danger",
              "url-filter-is-case-sensitive": false,
              "unless-top-url": [ "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?nice\\.com[:\\/]" ],
              "top-url-filter-is-case-sensitive": true,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      },
      "allow": [
        {
          "trigger": {
            "url-filter": "allowed",
            "url-filter-is-case-sensitive": false,
            "unless-top-url": [ "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?evil\\.com[:\\/]" ],
            "top-url-filter-is-case-sensitive": true,
            "resource-type": [
              "script"
            ]
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        }
      ]
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, BlockRuleWithInclusionsAndExclusions) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse(
      "something$domain=evil.com|~nice.com|~good.evil.com|except.good.evil.com|"
      "bad.evil.com|danger.com|~safe.danger.com|~allowed.safe.danger.com,"
      "script");
  std::string expected(R"===({
    "network": {
      "block-allow-pairs": [
        [
          {
            "trigger": {
              "url-filter": "something",
              "url-filter-is-case-sensitive": false,
              "if-top-url": [
                "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?danger\\.com[:\\/]" ,
                "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?evil\\.com[:\\/]"
              ],
              "top-url-filter-is-case-sensitive": true,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          },
          {
            "trigger": {
              "url-filter": "something",
              "url-filter-is-case-sensitive": false,
              "if-top-url": [
                "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?safe\\.danger\\.com[:\\/]",
                "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?good\\.evil\\.com[:\\/]"
              ],
              "top-url-filter-is-case-sensitive": true,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "ignore-previous-rules"
            }
          }
        ],
        [
          {
            "trigger": {
              "url-filter": "something",
              "url-filter-is-case-sensitive": false,
              "if-top-url": [ "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?except\\.good\\.evil\\.com[:\\/]" ],
              "top-url-filter-is-case-sensitive": true,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      ]
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, BlockRuleWithInclusionsCancelledByExclusions) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("something$domain=example.com|~example.com");
  std::string expected(R"===({
    "network": {},
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, BlockRuleWithSuperfluousExclusions) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("something$domain=evil.com|~nice.com, script");
  std::string expected(R"===({
    "network": {
      "block": {
        "specific": [
          {
            "trigger": {
              "url-filter": "something",
              "url-filter-is-case-sensitive": false,
              "if-top-url": [ "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?evil\\.com[:\\/]" ],
              "top-url-filter-is-case-sensitive": true,
              "resource-type": [
                "script"
              ]
            },
            "action": {
              "type": "block"
            }
          }
        ]
      }
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, AllowRuleWithInclusionsAndExclusions) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse(
      "@@something$domain=nice.com|~bad.nice.com|except.bad.nice.com|foo."
      "except.bad.nice.com|good.com|~evil.com|, script");
  std::string expected(R"===({
    "network": {
      "allow": [
        {
          "trigger": {
            "url-filter": "something",
            "url-filter-is-case-sensitive": false,
            "if-top-url": [
              "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?good\\.com[:\\/]",
              "^[a-z][a-z0-9.+-]*:(\\/\\/)?([^\\/]+@)?nice\\.com[:\\/]",
              "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?except\\.bad\\.nice\\.com[:\\/]"
             ],
            "top-url-filter-is-case-sensitive": true,
            "resource-type": [
              "script"
            ]
          },
          "action": {
            "type": "ignore-previous-rules"
          }
        }
      ]
    },
    "cosmetic": {},
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, GenericCosmeticHideRule) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("##.adfoo");
  rule_parser.Parse("##.adbar");
  std::string expected(R"===({
    "network": {},
    "cosmetic": {
        "selector": {
        ".adbar" : {
          "block": {
            "generic" : [
              {
                "trigger": {
                  "url-filter": ".*",
                  "url-filter-is-case-sensitive": false
                },
                "action": {
                  "type": "css-display-none",
                  "selector": ".adbar"
                }
              }
            ]
          }
        },
        ".adfoo" : {
          "block": {
            "generic" : [
            {
                "trigger": {
                  "url-filter": ".*",
                  "url-filter-is-case-sensitive": false
                },
                "action": {
                  "type": "css-display-none",
                  "selector": ".adfoo"
                }
              }
            ]
          }
        }
      }
    },
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, SpecificCosmeticHideRule) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("example.com##.ad");
  std::string expected(R"===({
    "network": {},
    "cosmetic": {
        "selector": {
        ".ad": {
          "block": {
            "specific": [
              {
                "trigger": {
                  "url-filter": ".*",
                  "url-filter-is-case-sensitive": false,
                  "if-top-url": [
                    "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?example\\.com[:\\/]"
                  ],
                  "top-url-filter-is-case-sensitive": true
                },
                "action": {
                  "type": "css-display-none",
                  "selector": ".ad"
                }
              }
            ]
          }
        }
      }
    },
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}

TEST(AdBlockIosRuleCompilerTest, CosmeticAllowRule) {
  ParseResult parse_result;
  RuleParser rule_parser(&parse_result, {});
  rule_parser.Parse("example.com#@#.show");
  rule_parser.Parse("#@#.nice");
  std::string expected(R"===({
    "network": {},
    "cosmetic": {
      "selector": {
        ".show": {
          "allow": [
            {
              "trigger": {
                "url-filter": ".*",
                "url-filter-is-case-sensitive": false,
                "if-top-url": [
                  "^[a-z][a-z0-9.+-]*:(\\/\\/)?(([^\\/]+@)?[^@:\\/\\[]+\\.)?example\\.com[:\\/]"
                ],
                "top-url-filter-is-case-sensitive": true
              },
              "action": {
                "type": "ignore-previous-rules"
              }
            }
          ]
        },
        ".nice": {
          "allow": [
            {
              "trigger": {
                "url-filter": ".*",
                "url-filter-is-case-sensitive": false
              },
              "action": {
                "type": "ignore-previous-rules"
              }
            }
          ]
        }
      }
    },
    "version": 1
  })===");
  ASSERT_TRUE(FormatJSON(expected));
  EXPECT_EQ(CompileIosRulesToString(parse_result, true), expected);
}
}  // namespace adblock_filter
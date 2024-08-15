// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/parse_utils.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace adblock_filter {
std::string BuildNgramSearchString(const std::string_view& pattern) {
  // Build a suitable search string for NGrams, allowing for indexing and fast
  // retrieval of the pattern when matching the url.
  // The '*' wildcard is treated as a separator during NGram search, so we use
  // it to mark anything that is possibly unknown.
  // The goal is therefore to get pieces of string that must appear in any
  // matched URL, separated by *, which let us be pretty loose with our
  // parsing of the regex.
  bool done = false;
  std::string ngram_search_string;
  for (auto c = pattern.begin(); c < pattern.end() && !done; c++) {
    switch (*c) {
      case '|':
        // Alternatives at the top level means we can't easily find a substring
        // that must be matched as we'd need to extract identical substrings
        // that must appear in all alternatives, which is unlikely to happen
        // with a well constructed regex anyway. So, we just give up.
        ngram_search_string.clear();
        done = true;
        break;
      case '^':
      case '$':
        break;
      case '(': {
        // We just ignore anything in subexpressions
        int paren_level = 1;
        while (++c != pattern.end()) {
          if (*c == '(' && *(c - 1) != '\\')
            paren_level++;
          if (*c == ')' && *(c - 1) != '\\')
            paren_level--;
          if (paren_level == 0)
            break;
        }
        ngram_search_string.push_back('*');
        break;
      }
      case '[':
        while (++c < pattern.end()) {
          if (*c == ']' && *(c - 1) != '\\')
            break;
        }
        [[fallthrough]];
      case '.':
        ngram_search_string.push_back('*');
        break;
      case '{':
        // Don't try to work out numbers. Assume the previous
        // assume the previous character doesn't need to be pre-matched.
        while (++c < pattern.end()) {
          if (*c == '}' && *(c - 1) != '\\')
            break;
        }
        [[fallthrough]];
      case '*':
      case '?':
        if (ngram_search_string.empty())
          break;
        ngram_search_string.pop_back();
        [[fallthrough]];
      case '+':
        ngram_search_string.push_back('*');
        break;
      case '\\':
        if (++c == pattern.end())
          break;
        if (base::IsAsciiAlpha(*c) || base::IsHexDigit(*c)) {
          // Assume an escape sequence that can match multiple characters.
          // Technically, it could be a control character, but that's not
          // valid in urls anyway.
          ngram_search_string.push_back('*');
          // assume any hex digit following is for a \x or \u - like sequence.
          while (++c < pattern.end()) {
            if (!base::IsHexDigit(*c)) {
              // compensate the end-of-loop increment
              --c;
              break;
            }
          }
          break;
        }
        // backslash used for escaping
        [[fallthrough]];
      default:
        ngram_search_string.push_back(*c);
    }
  }
  // get rid of consecutive, leading and trailing '*'
  ngram_search_string.erase(
      std::unique(ngram_search_string.begin(), ngram_search_string.end(),
                  [](char c1, char c2) { return c1 == '*' && c2 == '*'; }),
      ngram_search_string.end());

  std::string result;
  base::TrimString(ngram_search_string, "*", &result);
  return base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(result)));
}

}  // namespace adblock_filter

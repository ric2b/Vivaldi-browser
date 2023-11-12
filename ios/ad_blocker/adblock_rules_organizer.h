// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_AD_BLOCKER_ADBLOCK_RULE_ORGANIZER_H_
#define IOS_AD_BLOCKER_ADBLOCK_RULE_ORGANIZER_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"

namespace adblock_filter {
class CompiledRules : public base::RefCountedThreadSafe<CompiledRules> {
 public:
  CompiledRules(base::Value rules, std::string checksum);
  CompiledRules(const CompiledRules&) = delete;
  CompiledRules& operator=(const CompiledRules&) = delete;

  const base::Value& rules() const { return rules_; }
  const std::string& checksum() const { return checksum_; }

 private:
  friend class base::RefCountedThreadSafe<CompiledRules>;
  virtual ~CompiledRules();

  const base::Value rules_;
  const std::string checksum_;
};

base::Value OrganizeRules(
    std::map<uint32_t, scoped_refptr<CompiledRules>>,
    base::Value exception_rule);
}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_ADBLOCK_RULE_ORGANIZER_H_

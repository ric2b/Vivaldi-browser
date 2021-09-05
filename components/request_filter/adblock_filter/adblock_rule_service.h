// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/request_filter/adblock_filter/adblock_metadata.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

namespace adblock_filter {
class KnownRuleSourcesHandler;
class BlockedUrlsReporter;
class CosmeticFilter;

class RuleService : public KeyedService {
 public:
  enum ExceptionsList {
    kProcessList = 0,
    kExemptList,
    kFirstExceptionList = kProcessList,
    kLastExceptionList = kExemptList,
  };
  static constexpr size_t kExceptionListCount = kLastExceptionList + 1;

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    virtual void OnRuleServiceStateLoaded(RuleService* rule_service) {}
    // The properties of a rule source have been updated. Either because a
    // fetch started or completed.
    virtual void OnRulesSourceUpdated(const RuleSource& rule_source) {}

    virtual void OnRuleSourceDeleted(uint32_t source_id, RuleGroup group) {}

    // This is called when enabling/disabling groups or changing active
    // exception list
    virtual void OnGroupStateChanged(RuleGroup group) {}

    virtual void OnExceptionListChanged(RuleGroup group, ExceptionsList list) {}
  };

  class Delegate {
   public:
    virtual ~Delegate();
    virtual std::string GetLocaleForDefaultLists() = 0;
    // The delegate isn't expected to be called after this.
    virtual void RuleServiceDeleted() = 0;
  };

  ~RuleService() override;

  // Allows the service to query state from outside its world. The delegate
  // should only be set once.
  virtual void SetDelegate(Delegate* delegate) = 0;

  virtual bool IsLoaded() const = 0;

  virtual bool IsRuleGroupEnabled(RuleGroup group) const = 0;
  virtual void SetRuleGroupEnabled(RuleGroup group, bool enabled) = 0;

  // Adds a rules source from the given URL. Returns the ID attributed to it or
  // nothing if the same rule source was already added.
  virtual base::Optional<uint32_t> AddRulesFromURL(RuleGroup group,
                                                   const GURL& url) = 0;
  // Adds a rules source from the given file. Returns the ID attributed to it or
  // nothing if the same rule source was already added.
  virtual base::Optional<uint32_t> AddRulesFromFile(
      RuleGroup group,
      const base::FilePath& file) = 0;

  // Returns the rule source matching the given ID, if it is an existing ID.
  virtual base::Optional<RuleSource> GetRuleSource(RuleGroup group,
                                                   uint32_t source_id) = 0;
  virtual std::map<uint32_t, RuleSource> GetRuleSources(
      RuleGroup group) const = 0;

  // Triggers an immediate fetching of a rule source instead of waiting for its
  // next update time.
  virtual bool FetchRuleSourceNow(RuleGroup group, uint32_t source_id) = 0;

  // Removes a rule source.
  virtual void DeleteRuleSource(RuleGroup group, uint32_t source_id) = 0;

  virtual void SetActiveExceptionList(RuleGroup group, ExceptionsList list) = 0;
  virtual ExceptionsList GetActiveExceptionList(RuleGroup group) const = 0;

  virtual void AddExceptionForDomain(RuleGroup group,
                                     ExceptionsList list,
                                     const std::string& domain) = 0;
  virtual void RemoveExceptionForDomain(RuleGroup group,
                                        ExceptionsList list,
                                        const std::string& domain) = 0;
  virtual void RemoveAllExceptions(RuleGroup group, ExceptionsList list) = 0;
  virtual const std::set<std::string>& GetExceptions(
      RuleGroup group,
      ExceptionsList list) const = 0;

  // This returns whether a given origin will be subject to filtering in a given
  // group or not, based on the active exception list.
  virtual bool IsExemptOfFiltering(RuleGroup group,
                                   url::Origin origin) const = 0;

  // Check if a given document |url| is blocked to determine whether to show
  // an interstiatial in the given |frame|.
  virtual bool IsDocumentBlocked(RuleGroup group,
                                 content::RenderFrameHost* frame,
                                 const GURL& url) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets the checksum of the index used for fast-finding of the rules.
  // This will be an empty string until an index gets built for the first time.
  // If it remains empty or becomes empty later on, this means saving the index
  // to disk is failing.
  virtual std::string GetRulesIndexChecksum(RuleGroup group) = 0;

  virtual KnownRuleSourcesHandler* GetKnownSourcesHandler() = 0;
  virtual BlockedUrlsReporter* GetBlockerUrlsReporter() = 0;

  // Helper method for setting up a new cosmetic filter with the needed indexes.
  virtual void InitializeCosmeticFilter(CosmeticFilter* filter) = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_H_

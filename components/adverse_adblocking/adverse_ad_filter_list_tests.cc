// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/adverse_adblocking/adverse_ad_filter_list.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/vivaldi_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const std::string kAdversAdFilterListConfig(R"(
[
    {
        "abusiveStatus": "FAILING",
        "enforcementTime": "2019-02-02T18:15:09.101769Z",
        "filterStatus": "ON",
        "lastChangeTime": "2019-01-03T18:15:09.935661Z",
        "reportUrl": null,
        "reviewedSite": "subdomain.example.com",
        "underReview": null
    },
    {
        "abusiveStatus": "FAILING",
        "enforcementTime": "2019-02-15T19:13:01.033012Z",
        "filterStatus": "PENDING",
        "lastChangeTime": "2019-01-16T19:13:01.136057Z",
        "reportUrl": null,
        "reviewedSite": "example.org",
        "underReview": null
    }
])");

const base::FilePath::StringType kAdverseAdFilterJsonFileName =
    FILE_PATH_LITERAL("adverse_ad_filter1.json");

GURL CreateURL(const std::string& hostname) {
  return GURL("https://" + hostname);
}

base::FilePath GetAdfilterPath(const base::FilePath::StringType& filename) {
  base::FilePath test_data_dir;
  if (!base::PathService::Get(vivaldi::DIR_VIVALDI_TEST_DATA, &test_data_dir)) {
    ADD_FAILURE() << "Test Data dir retrieval failed '"
                  << test_data_dir.AsUTF8Unsafe() << "'";
    return base::FilePath();
  }
  return test_data_dir.Append(FILE_PATH_LITERAL("components"))
      .Append(FILE_PATH_LITERAL("adverse_ad_blocking"))
      .Append(filename);
}

}  // namespace

class AdverseAdFiltering : public ::testing::Test {
 public:
  void SetUp() override {
    vivaldi::RegisterVivaldiPaths();
    ::testing::Test::SetUp();
  }

  std::unique_ptr<AdverseAdFilterListService> filter_list_;
};

TEST_F(AdverseAdFiltering, LoadString) {
  filter_list_.reset(new AdverseAdFilterListService(nullptr));

  filter_list_->LoadAndInitializeFromString(&kAdversAdFilterListConfig);

  ASSERT_TRUE(filter_list_->has_sites());

  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("www.vivaldi.net")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("vivaldi.com")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("google.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("subdomain.example.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("subdomain.example.org")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("example.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("example.org")));
}

TEST_F(AdverseAdFiltering, LoadFile) {
  filter_list_.reset(new AdverseAdFilterListService(nullptr));

  // constructed loading from file
  std::string* json_string = AdverseAdFilterListService::ReadFileToString(
      GetAdfilterPath(kAdverseAdFilterJsonFileName));

  filter_list_->LoadAndInitializeFromString(json_string);

  ASSERT_TRUE(filter_list_->has_sites());

  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("www.vivaldi.net")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("vivaldi.com")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("google.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("subdomain.example.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("subdomain.example.org")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("example.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("example.org")));
}

TEST_F(AdverseAdFiltering, AddHost) {
  filter_list_.reset(new AdverseAdFilterListService(nullptr));

  filter_list_->AddBlockItem("subdomain.example.com");
  filter_list_->AddBlockItem("example.org");

  ASSERT_TRUE(filter_list_->has_sites());

  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("www.vivaldi.net")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("vivaldi.com")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("google.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("subdomain.example.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("subdomain.example.org")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("example.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("example.org")));
}

TEST_F(AdverseAdFiltering, AddHostThenClear) {
  filter_list_.reset(new AdverseAdFilterListService(nullptr));

  filter_list_->AddBlockItem("subdomain.example.com");
  filter_list_->AddBlockItem("example.org");

  ASSERT_TRUE(filter_list_->has_sites());

  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("www.vivaldi.net")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("vivaldi.com")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("google.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("subdomain.example.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("subdomain.example.org")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("example.com")));
  ASSERT_TRUE(filter_list_->IsSiteInList(CreateURL("example.org")));

  filter_list_->ClearSiteList();

  ASSERT_FALSE(filter_list_->has_sites());

  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("www.vivaldi.net")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("vivaldi.com")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("google.com")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("subdomain.example.com")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("subdomain.example.org")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("example.com")));
  ASSERT_FALSE(filter_list_->IsSiteInList(CreateURL("example.org")));
}

TEST_F(AdverseAdFiltering, LotsAurls) {
  base::Time start_time = base::Time::Now();
  filter_list_.reset(new AdverseAdFilterListService(nullptr));

  int i;
  for (i = 0; i < 1000000; i++) {
    {
      std::ostringstream domain_name;
      domain_name << "subdomain-" << i << ".example.com";
      filter_list_->AddBlockItem(domain_name.str());
    }
  }
  base::Time init_time = base::Time::Now();
  LOG(INFO) << "Init time " << (init_time - start_time).InMilliseconds()
            << " ms";

  std::ostringstream domain_prefix;
  for (i = 0; i < 50; i++) {
    domain_prefix << "foo" << i << ".";
  }

  for (i = 0; i < 1000000; i += 3) {
    {
      std::ostringstream domain_name;
      domain_name << "subdomain-" << i << ".example.com";
      ASSERT_TRUE(filter_list_->IsSiteInList(
          CreateURL(domain_prefix.str() + domain_name.str())))
          << domain_prefix.str() + domain_name.str();
    }
    {
      std::ostringstream domain_name;
      domain_name << "subdomain-" << i + 1 << ".example.org";
      ASSERT_FALSE(filter_list_->IsSiteInList(
          CreateURL(domain_prefix.str() + domain_name.str())))
          << domain_prefix.str() + domain_name.str();
    }
  }
  base::Time test_time = base::Time::Now();
  LOG(INFO) << "Test time " << (test_time - init_time).InMilliseconds()
            << " ms";
  LOG(INFO) << "Total time " << (test_time - start_time).InMilliseconds()
            << " ms";
}

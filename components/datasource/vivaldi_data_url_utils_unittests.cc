// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/datasource/vivaldi_data_url_utils.h"

#include "app/vivaldi_constants.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vivaldi_data_url_utils {

namespace {

const PathType kTypeList[PathTypeCount] = {
    PathType::kLocalPath,        PathType::kImage,
    PathType::kCSSMod,           PathType::kSyncedStore,
    PathType::kDesktopWallpaper,
};

}  // namespace

class VivaldiDataUrlUtilsTest : public testing::Test {};

TEST_F(VivaldiDataUrlUtilsTest, ParsePath) {
  std::string data;
  std::optional<PathType> type;

  // Check for parsing of all known types.
  for (PathType i : kTypeList) {
    std::string dir = top_dir(i);
    type = ParsePath("/" + dir + "/some_id", &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ("some_id", data);

    // Data can be empty.
    type = ParsePath("/" + dir + "/", &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ(data, "");

    type = ParsePath("/" + dir, &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ(data, "");

    // Data can contain slashes.
    type = ParsePath("/" + dir + "/test/foo/bar/", &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ(data, "test/foo/bar/");

    // The query should be striped
    type = ParsePath("/" + dir + "/testdata?query", &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ(data, "testdata");

    type = ParsePath("/" + dir + "/testdata?", &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ(data, "testdata");

    type = ParsePath("/" + dir + "/testdata/?", &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ(data, "testdata/");
  }
}

TEST_F(VivaldiDataUrlUtilsTest, ParsePath_BadFormat) {
  std::string data;
  std::optional<PathType> type;

  // In the invalid format checks use the name of one of top directories to
  // ensure that the url is rejected due to bad format, not an unknwon
  // directory.
  EXPECT_EQ(std::string_view(top_dir(PathType::kImage)), "thumbnail");

  // The path cannot be empty.
  type = ParsePath("");
  EXPECT_EQ(type, std::nullopt);

  // The path must be absolute.
  type = ParsePath("thumbnail");
  EXPECT_EQ(type, std::nullopt);

  type = ParsePath("thumbnail/id.png");
  EXPECT_EQ(type, std::nullopt);

  // An unknown top directory must be rejected.
  type = ParsePath("/unknown/data");
  EXPECT_EQ(type, std::nullopt);

  type = ParsePath("/thumbnail2/data");
  EXPECT_EQ(type, std::nullopt);

  type = ParsePath("/thumbnail2");
  EXPECT_EQ(type, std::nullopt);
}

TEST_F(VivaldiDataUrlUtilsTest, ParsePath_OldFormats) {
  std::string data;
  std::optional<PathType> type;

  // thumbnail and local-image specific checks to ensure that we still support
  // the older forms.
  EXPECT_EQ(std::string_view(top_dir(PathType::kImage)), "thumbnail");
  EXPECT_EQ(std::string_view(top_dir(PathType::kLocalPath)), "local-image");

  // Check that parsing of the old thumbnail url format works.
  type = ParsePath("/http://bookmark_thumbnail/id?query", &data);
  EXPECT_EQ(type, PathType::kImage);
  EXPECT_EQ(data, "id");

  // Check the parsing of old thumbnail path stored as local-image with data
  // that must be positive int63. The data should be converted into an actual
  // file name by appending the .png suffix.
  type = ParsePath("/local-image/1", &data);
  EXPECT_EQ(type, PathType::kImage);
  EXPECT_EQ(data, "1.png");

  // Check max int64.
  type = ParsePath("/local-image/9223372036854775807?something", &data);
  EXPECT_EQ(type, PathType::kImage);
  EXPECT_EQ(data, "9223372036854775807.png");

  // Non-positive integers outside int63 range are data for local-image.
  type = ParsePath("/local-image/-42?query", &data);
  EXPECT_EQ(type, PathType::kLocalPath);
  EXPECT_EQ(data, "-42");

  type = ParsePath("/local-image/0", &data);
  EXPECT_EQ(type, PathType::kLocalPath);
  EXPECT_EQ(data, "0");

  // max_int63 + 1 should mean the local path.
  type = ParsePath("/local-image/9223372036854775808", &data);
  EXPECT_EQ(type, PathType::kLocalPath);
  EXPECT_EQ(data, "9223372036854775808");
}

TEST_F(VivaldiDataUrlUtilsTest, UrlParse) {
  std::string data;
  std::optional<PathType> type;

  // This does not test parsing of the path as the PathTest covers that.

  // Check that invalid urls including relative forms are reported.
  type = ParseUrl("");
  EXPECT_EQ(type, std::nullopt);

  type = ParseUrl("thumbnail");
  EXPECT_EQ(type, std::nullopt);

  type = ParseUrl("/thumbnail/data.png");
  EXPECT_EQ(type, std::nullopt);

  type = ParseUrl("//vivaldi-data/thumbnail/data.png");
  EXPECT_EQ(type, std::nullopt);

  // An unknown or wrong schema should be reported.
  type = ParseUrl("foo://vivaldi-data/thumbnail/data.png");
  EXPECT_EQ(type, std::nullopt);

  type = ParseUrl("https://vivaldi-data/thumbnail/data.png");
  EXPECT_EQ(type, std::nullopt);

  // Check that an older thumb host is an alias for vivaldi-data.
  for (PathType i : kTypeList) {
    std::string path = std::string("/") + top_dir(i) + "/some_id";
    type = ParseUrl("chrome://vivaldi-data" + path, &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ("some_id", data);
    type = ParseUrl("chrome://thumb" + path, &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ("some_id", data);
  }

  // Check that ? is ignored.
  type = ParseUrl("chrome://vivaldi-data/thumbnail/data.png?query", &data);
  EXPECT_EQ(type, PathType::kImage);
  EXPECT_EQ("data.png", data);
}

TEST_F(VivaldiDataUrlUtilsTest, MakeUrl) {
  std::string data;
  std::optional<PathType> type;

  for (PathType i : kTypeList) {
    std::string url = MakeUrl(i, "data");
    EXPECT_TRUE(base::StartsWith(url, vivaldi::kVivaldiUIDataURL));
    EXPECT_TRUE(base::EndsWith(url, "/data"));
    type = ParseUrl(url, &data);
    EXPECT_EQ(type, i);
    EXPECT_EQ("data", data);
  }
}

}  // namespace vivaldi_data_url_utils
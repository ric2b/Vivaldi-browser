// Copyright (c) 2014 Vivaldi Technologies AS. All rights reserved

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/vivaldi_paths.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/importer_unittest_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/test/browser_test.h"
#include "importer/imported_notes_entry.h"

#include "testing/gtest/include/gtest/gtest.h"

using base::PathService;

namespace {
const size_t kMaxPathLen = 5;

struct OperaPasswordInfo {
  const bool wildcard;
  const password_manager::PasswordForm::Scheme scheme;
  const char* url;
  const char* realm;
  const wchar_t* username_field;
  const wchar_t* username;
  const wchar_t* password_field;
  const wchar_t* password;
  const bool blocked_by_user;
};

struct OperaBookmarkInfo {
  const bool is_folder;
  const bool in_toolbar;
  const bool is_speeddial;
  const size_t path_len;
  const char* path[kMaxPathLen];
  const wchar_t* title;
  const char* url;
};

struct OperaNotesInfo {
  const bool is_folder;
  const size_t path_len;
  const char* path[kMaxPathLen];
  const wchar_t* title;
  const char* url;
  const wchar_t* content;
};

const wchar_t kTestMasterPassword[] = L"0perav1v";

const OperaPasswordInfo OperaPasswords[] = {
    {false, password_manager::PasswordForm::Scheme::kHtml,
     "http://localhost:8081/login", "http://localhost:8081/", L"username",
     L"user1", L"password", L"password1", false},
    {false, password_manager::PasswordForm::Scheme::kHtml,
     "http://localhost:8082/login", "http://localhost:8082/", L"username",
     L"user2", L"password", L"password2", false},
};

const OperaBookmarkInfo OperaBookmarks[] = {
    {false,
     false,
     false,
     0,
     {},
     L"Vivaldi.net - Welcome",
     "https://vivaldi.net/en-US/"},
    {true, false, false, 0, {}, L"folder 1", NULL},
    {false,
     false,
     false,
     1,
     {"folder 1"},
     L"Coming Soon",
     "http://vivaldi.com/"},
    {true, false, false, 0, {}, L"folder 2", NULL},
    {false,
     false,
     false,
     1,
     {"folder 2"},
     L"Opera-nettleser - Den alternative nettleseren - Last ned gratis",
     "http://www.opera.com/no"},
    {false,
     false,
     false,
     1,
     {"folder 2"},
     L"Google",
     "https://www.google.com/"},
};

const OperaNotesInfo OperaNotes[] = {
    {false, 0, {}, L"Note 1", NULL, L"Note 1\n\nA test of content"},
    {false,
     0,
     {},
     L"Forums",
     "https://vivaldi.net/en-US/",
     L"Forums\n\nParticipate in discussions with others or create your own"},
    {true, 0, {}, L"folder1", NULL, NULL},
    {false,
     1,
     {"folder1"},
     L"Note 2",
     NULL,
     L"Note 2\n\nTest of a note in subfolder"},
    {false,
     1,
     {"folder1"},
     L"Note 3",
     NULL,
     L"Note 3\n\nAnother test of a note in subfolder\n"},
    {true, 1, {"folder1"}, L"folder 2", "", L""},
    {false,
     2,
     {"folder1", "folder 2"},
     L"Photos",
     "https://vivaldi.net/en-US/",
     L"Photos\n\nShare your photos with friends and family"},
};

void TestImportedBookmarks(const ImportedBookmarkEntry& imported,
                           const OperaBookmarkInfo& expected) {
  EXPECT_EQ(base::WideToUTF16(expected.title), imported.title);
  EXPECT_EQ(expected.is_folder, imported.is_folder) << imported.title;
  EXPECT_EQ(expected.in_toolbar, imported.in_toolbar) << imported.title;
  EXPECT_EQ(expected.is_speeddial, imported.speeddial) << imported.title;
  EXPECT_EQ(expected.path_len, imported.path.size()) << imported.title;
  EXPECT_EQ((expected.url ? expected.url : ""), imported.url.spec())
      << imported.title;
  for (size_t i = 0; i < expected.path_len; i++) {
    EXPECT_EQ(base::ASCIIToUTF16(expected.path[i]), imported.path[i])
        << imported.title;
  }
}

void TestImportedNotes(const ImportedNotesEntry& imported,
                       const OperaNotesInfo& expected) {
  EXPECT_EQ(base::WideToUTF16(expected.title), imported.title);
  EXPECT_EQ(base::WideToUTF16(expected.content ? expected.content : L""),
            imported.content)
      << imported.title;
  EXPECT_EQ(expected.is_folder, imported.is_folder) << imported.title;
  EXPECT_EQ(expected.path_len, imported.path.size()) << imported.title;
  EXPECT_EQ((expected.url ? expected.url : ""), imported.url.spec())
      << imported.title;
  for (size_t i = 0; i < expected.path_len; i++) {
    EXPECT_EQ(base::ASCIIToUTF16(expected.path[i]), imported.path[i])
        << imported.title;
  }
}

class OperaImportObserver : public ProfileWriter,
                            public importer::ImporterProgressObserver {
 public:
  OperaImportObserver(base::RunLoop *loop)
      : ProfileWriter(NULL),
        loop(loop),
        bookmark_count(0),
        notes_count(0),
        password_count(0) {}

  void ImportStarted() override {}
  void ImportItemStarted(importer::ImportItem item) override {}
  void ImportItemEnded(importer::ImportItem item) override {}
  void ImportEnded() override {
    loop->Quit();
    EXPECT_EQ(std::size(OperaBookmarks), bookmark_count);
    EXPECT_EQ(std::size(OperaNotes), notes_count);
    EXPECT_EQ(std::size(OperaPasswords), password_count);
  }

  bool BookmarkModelIsLoaded() const override { return true; }

  bool TemplateURLServiceIsLoaded() const override { return true; }

  void AddPasswordForm(const password_manager::PasswordForm& form) override {
    const OperaPasswordInfo& p = OperaPasswords[password_count];
    // EXPECT_EQ(p.wildcard,form.);
    EXPECT_EQ(p.scheme, form.scheme);
    EXPECT_EQ(p.url, form.url.spec());
    EXPECT_EQ((p.realm ? p.realm : ""), form.signon_realm);
    EXPECT_EQ(base::WideToUTF16(p.username_field), form.username_element);
    EXPECT_EQ(base::WideToUTF16(p.username), form.username_value);
    EXPECT_EQ(base::WideToUTF16(p.password_field), form.password_element);
    EXPECT_EQ(base::WideToUTF16(p.password), form.password_value);
    EXPECT_EQ(p.blocked_by_user, form.blocked_by_user);
    ++password_count;
  }
  void AddBookmarks(const std::vector<ImportedBookmarkEntry>& bookmarks,
                    const std::u16string& top_level_folder_name) override {
    ASSERT_LE(bookmark_count + bookmarks.size(), std::size(OperaBookmarks));

    for (size_t i = 0; i < bookmarks.size(); i++) {
      EXPECT_NO_FATAL_FAILURE(
          TestImportedBookmarks(bookmarks[i], OperaBookmarks[bookmark_count]))
          << i;
      ++bookmark_count;
    }
  }
  void AddNotes(const std::vector<ImportedNotesEntry>& notes,
                const std::u16string& top_level_folder_name) override {
    ASSERT_LE(notes_count + notes.size(), std::size(OperaNotes));

    for (size_t i = 0; i < notes.size(); i++) {
      EXPECT_NO_FATAL_FAILURE(
          TestImportedNotes(notes[i], OperaNotes[notes_count]))
          << i;
      ++notes_count;
    }
  }

 private:
  ~OperaImportObserver() override {}

  base::RunLoop *loop;
  size_t bookmark_count;
  size_t notes_count;
  size_t password_count;
};
}  // namespace

class OperaProfileImporterBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    vivaldi::RegisterVivaldiPaths();
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath temp = temp_dir.GetPath().AppendASCII("OperaImportTest");
    base::DeleteFile(temp);
    base::CreateDirectory(temp);
    profile_dir = temp.AppendASCII("profile");

    InProcessBrowserTest::SetUp();
  }

  void TestVivaldiImportOfOpera(std::string profile_subdir,
                                bool use_master_password = false) {
    // CopyDirectory requires IO access
    base::RunLoop loop;
    scoped_refptr<OperaImportObserver> observer(new OperaImportObserver(&loop));

    base::VivaldiScopedAllowBlocking allow_blocking;
    base::FilePath data_dir;
    ASSERT_TRUE(PathService::Get(vivaldi::DIR_VIVALDI_TEST_DATA, &data_dir));
    data_dir = data_dir.AppendASCII("importer");
    data_dir = data_dir.AppendASCII(profile_subdir);
    ASSERT_TRUE(base::CopyDirectory(data_dir, profile_dir, true));

    importer::SourceProfile import_profile;
    import_profile.importer_type = importer::TYPE_OPERA;
    import_profile.source_path = profile_dir;
    import_profile.locale = "en-US";
    if (use_master_password) {
      import_profile.master_password = base::WideToUTF8(kTestMasterPassword);
    }

    uint16_t imported_items =
        importer::PASSWORDS | importer::NOTES | importer::FAVORITES;

    // Deletes itself
    ExternalProcessImporterHost* host = new ExternalProcessImporterHost;

    host->set_observer(observer.get());
    host->StartImportSettings(
        import_profile, browser()->profile(), imported_items,
        base::WrapRefCounted<ProfileWriter>(observer.get()).get());

    loop.Run();
  }

  base::ScopedTempDir temp_dir;
  base::FilePath profile_dir;
};

IN_PROC_BROWSER_TEST_F(OperaProfileImporterBrowserTest,
                       ImportNoMasterPassword) {
  TestVivaldiImportOfOpera("opera-nopass");
}

IN_PROC_BROWSER_TEST_F(OperaProfileImporterBrowserTest,
                       ImportWithMasterPassword) {
  TestVivaldiImportOfOpera("opera-pass", true);
}

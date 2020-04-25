// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/css_mods_data_source.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

CSSModsDataClassHandler::CSSModsDataClassHandler(Profile* profile)
    : profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CSSModsDataClassHandler::~CSSModsDataClassHandler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

bool CSSModsDataClassHandler::GetData(
    const std::string& data_id,
    const content::URLDataSource::GotDataCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath custom_css_path = profile_->GetPrefs()->GetFilePath(
      vivaldiprefs::kAppearanceCssUiModsDirectory);
  if (custom_css_path.empty())
    return false;

  // If callback was OnceCallback, we could pass it directly as the result
  // argument to PostTask. But it is RepeatingCallback. Thus we use a helper
  // method to redirect to it.
  return base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CSSModsDataClassHandler::GetDataForIdOnBlockingThread,
                     custom_css_path, data_id),
      base::BindOnce(&CSSModsDataClassHandler::PostResultsOnThread, callback));
}

// static
scoped_refptr<base::RefCountedMemory>
CSSModsDataClassHandler::GetDataForIdOnBlockingThread(base::FilePath dir_path,
                                                      std::string data_id) {
  if (data_id == "css") {
    std::string data;
    std::vector<base::FilePath::StringType> files;
    base::FileEnumerator it(dir_path, false, base::FileEnumerator::FILES,
      FILE_PATH_LITERAL("*.css"));
    for (base::FilePath name = it.Next(); !name.empty(); name = it.Next()) {
      files.emplace_back(name.value());
    }
    // sort the files alphabetically.
    std::sort(files.begin(), files.end());
    for (auto& file : files) {
      base::FilePath path(file);
      // TODO(pettern): Using file urls would be best but is not allowed
      // in our app, investigate if and how it can be done.
      std::string import_statement = base::StringPrintf("@import url('%s');\n",
#if defined(OS_POSIX)
        path.BaseName().value().c_str()
#elif defined(OS_WIN)
        base::UTF16ToUTF8(path.BaseName().value()).c_str()
#endif
      );
      data.append(import_statement);
    }
    if (data.empty()) {
      data.append("{}");
    }
    return base::MakeRefCounted<base::RefCountedBytes>(
        reinterpret_cast<const unsigned char*>(data.data()),
        (size_t)data.length());
  } else {
    base::FilePath file_path = dir_path.AppendASCII(data_id);
    return extensions::VivaldiDataSourcesAPI::ReadFileOnBlockingThread(
        file_path);
  }
}

// static
void CSSModsDataClassHandler::PostResultsOnThread(
    const content::URLDataSource::GotDataCallback& callback,
    scoped_refptr<base::RefCountedMemory> data) {
  callback.Run(std::move(data));
}

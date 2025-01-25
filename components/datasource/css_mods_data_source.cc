// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/css_mods_data_source.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/datasource/vivaldi_data_url_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace {
const char kCSSModsData[] = "css";
const char kCSSModsExtension[] = ".css";
}  // namespace

void CSSModsDataClassHandler::GetData(
    Profile* profile,
    const std::string& data_id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath custom_css_path = profile->GetPrefs()->GetFilePath(
      vivaldiprefs::kAppearanceCssUiModsDirectory);
  if (custom_css_path.empty()) {
    std::string data = "{}";

    scoped_refptr<base::RefCountedMemory> memory =
        base::MakeRefCounted<base::RefCountedBytes>(
            base::span(reinterpret_cast<const unsigned char*>(data.data()),
                       (size_t)data.length()));

    std::move(callback).Run(std::move(memory));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CSSModsDataClassHandler::GetDataForIdOnBlockingThread,
                     custom_css_path, data_id),
      std::move(callback));
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
      std::string import_statement =
          base::StringPrintf("@import url('%s');\n",
#if BUILDFLAG(IS_POSIX)
                             path.BaseName().value().c_str()
#elif BUILDFLAG(IS_WIN)
                             base::WideToUTF8(path.BaseName().value()).c_str()
#endif
          );
      data.append(import_statement);
    }
    if (data.empty()) {
      data.append("{}");
    }
    return base::MakeRefCounted<base::RefCountedBytes>(
        base::span(reinterpret_cast<const unsigned char*>(data.data()),
        (size_t)data.length()));
  } else {
    base::FilePath file_path = dir_path.AppendASCII(data_id);
    return vivaldi_data_url_utils::ReadFileOnBlockingThread(file_path);
  }
}

std::string CSSModsDataClassHandler::GetMimetype(Profile* profile,
                                                 const std::string& data_id) {
  if (data_id == kCSSModsData || base::EndsWith(data_id, kCSSModsExtension))
    return "text/css";
  return vivaldi_data_url_utils::kMimeTypePNG;
}
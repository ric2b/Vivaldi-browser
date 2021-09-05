// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.

#include "components/datasource/desktop_data_source_win.h"

#include <Shobjidl.h>
#include <wrl/client.h>

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/skia_utils_win.h"

DesktopWallpaperDataClassHandlerWin::DesktopWallpaperDataClassHandlerWin() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

DesktopWallpaperDataClassHandlerWin::~DesktopWallpaperDataClassHandlerWin() {
  // See comments in VivaldiDataSource::~VivaldiDataSource why there is no race
  // with GetData method.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void DesktopWallpaperDataClassHandlerWin::GetData(
    const std::string& data_id,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  do {
    Microsoft::WRL::ComPtr<IDesktopWallpaper> desktop_w;
    HRESULT hr = CoCreateInstance(__uuidof(DesktopWallpaper), nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&desktop_w));
    if (FAILED(hr)) {
      break;
    }
    UINT count;
    hr = desktop_w->GetMonitorDevicePathCount(&count);
    if (FAILED(hr)) {
      break;
    }
    base::win::ScopedCoMem<wchar_t> file_path;
    base::win::ScopedCoMem<wchar_t> monitor_id;

    for (UINT n = 0; n < count; n++) {
      hr = desktop_w->GetMonitorDevicePathAt(
          n, reinterpret_cast<LPWSTR*>(&monitor_id));
      if (SUCCEEDED(hr)) {
        // Try first without monitor id, this will work if the user
        // has the same image on all monitors.
        hr = desktop_w->GetWallpaper(nullptr,
                                    reinterpret_cast<LPWSTR*>(&file_path));
        if (hr == S_FALSE) {
          file_path.Reset(nullptr);
          hr = desktop_w->GetWallpaper(monitor_id,
                                      reinterpret_cast<LPWSTR*>(&file_path));
        }
        if (SUCCEEDED(hr)) {
          if (file_path.get() == previous_path_) {
            // Path has not changed, serve cached data
            std::move(callback).Run(cached_image_data_);
            return;
          }
          // Unretained is used because Chromium's URLDataSource and so this
          // class is destroyed on UI thread strictly after all outstanding
          // GotDataCallback callbacks runs on UI thread.
          base::ThreadPool::PostTask(
              FROM_HERE,
              {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
               base::TaskPriority::USER_VISIBLE},
              base::BindOnce(
                  &DesktopWallpaperDataClassHandlerWin::GetDataOnFileThread,
                  base::Unretained(this), std::move(file_path.get()),
                  std::move(callback)));
          return;
        }
      }
    }
  } while (false);

  std::move(callback).Run(nullptr);
}

void DesktopWallpaperDataClassHandlerWin::SendDataResultsOnUiThread(
    scoped_refptr<base::RefCountedMemory> image_data,
    std::wstring path,
    content::URLDataSource::GotDataCallback callback) {
  cached_image_data_ = image_data;
  previous_path_ = path;

  std::move(callback).Run(image_data);
}

void DesktopWallpaperDataClassHandlerWin::GetDataOnFileThread(
    std::wstring file_path,
    content::URLDataSource::GotDataCallback callback) {
  base::FilePath f(file_path);
  base::File file(f, base::File::FLAG_READ | base::File::FLAG_OPEN);
  int64_t len = file.GetLength();
  if (len > 0) {
    std::vector<unsigned char> buffer(len);
    int read_len =
      file.Read(0, reinterpret_cast<char*>(&buffer[0]), len);
    if (read_len == len) {
      scoped_refptr<base::RefCountedMemory> image_data(
          base::RefCountedBytes::TakeVector(&buffer));
      // Unretained is used because Chromium's URLDataSource and so this
      // class is destroyed on UI thread strictly after all outstanding
      // GotDataCallback callbacks runs on UI thread.
      base::PostTask(
          FROM_HERE,
          {content::BrowserThread::UI},
          base::BindOnce(
              &DesktopWallpaperDataClassHandlerWin::SendDataResultsOnUiThread,
              base::Unretained(this), std::move(image_data),
              std::move(file_path), std::move(callback)));
    }
  }
}

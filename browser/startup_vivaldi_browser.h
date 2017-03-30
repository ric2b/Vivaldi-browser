
#ifndef BROWSER_STARTUP_VIVALDI_BROWSER_H
#define BROWSER_STARTUP_VIVALDI_BROWSER_H

#include <vector>
#include "url/gurl.h"

namespace base {
class CommandLine;
class FilePath;
} // namespace base

class Profile;

namespace vivaldi {
bool LaunchVivaldi(const base::CommandLine &command_line,
                   const base::FilePath &cur_dir,
                   Profile *profile);
bool AddVivaldiNewPage(bool  welcome_run_none,
                       std::vector<GURL>* startup_urls);

}  // namespace vivaldi

#endif // BROWSER_STARTUP_VIVALDI_BROWSER_H

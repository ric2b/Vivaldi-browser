/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2016 Vaclav Slavik
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_UPDATECHECKER_H_
#define UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_UPDATECHECKER_H_

#include "update_notifier/thirdparty/winsparkle/src/appcast.h"

#include <memory>
#include <string>

namespace winsparkle {

/**
    Compares versions @a a and @a b.

    The comparison is somewhat intelligent, it handles beta and RC
    components correctly.

    @return 0 if the versions are identical, negative value if
            @a a is smaller than @a b, positive value if @a a
            is larger than @a b.
 */
int CompareVersions(const std::string& a, const std::string& b);

time_t GetLastUpdateCheckTime();

// Check for updates and return a valid  appcast if so.
//
// manual - should be true if the user manually triggered the update check.
std::unique_ptr<Appcast> CheckForUpdates(bool manual, Error& error);

}  // namespace winsparkle

#endif  // UPDATE_NOTIFIER_THIRDPARTY_WINSPARKLE_SRC_UPDATECHECKER_H_

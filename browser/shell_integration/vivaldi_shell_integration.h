// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved

namespace vivaldi {

// Returns true if Chrome is likely to be the default browser for the current
// user. This method is very fast so it can be invoked in the UI thread.
bool IsChromeDefaultBrowser();

// Returns true if Opera is likely to be the default browser for the current
// user. This method is very fast so it can be invoked in the UI thread.
bool IsOperaDefaultBrowser();

}  //  namespace vivaldi

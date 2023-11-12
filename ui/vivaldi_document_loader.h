// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_DOCUMENT_LOADER_DOCUMENT_H_
#define VIVALDI_DOCUMENT_LOADER_DOCUMENT_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/extension.h"

// NOTE (andre@vivaldi.com) : Root level holder for all windows in Vivaldi for
// windows rendered through a portal.

class VivaldiDocumentLoader : protected content::WebContentsDelegate,
                              protected content::WebContentsObserver {
 public:
  VivaldiDocumentLoader(Profile* profile,
                        const extensions::Extension* extension);
  ~VivaldiDocumentLoader() override;

  content::WebContents* GetWebContents() {return vivaldi_web_contents_.get();}

  // Load the document into |vivaldi_web_contents_|
  void Load();

 private:

  // content::WebContentsDelegate overrides.
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  bool IsNeverComposited(content::WebContents* web_contents) override;

  // content::WebContentsObserver overrides.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::unique_ptr<content::WebContents> vivaldi_web_contents_;
  const raw_ptr<const extensions::Extension> vivaldi_extension_;

  // Timers.
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
};

#endif  // VIVALDI_DOCUMENT_LOADER_DOCUMENT_H_

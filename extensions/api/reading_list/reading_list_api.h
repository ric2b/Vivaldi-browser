// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_READING_LIST_API_H_
#define EXTENSIONS_API_READING_LIST_API_H_

#include "base/scoped_observation.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "content/public/browser/browser_context.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

class GURL;
class ReadingListModel;
class Profile;

namespace extensions {

class ReadingListPrivateAPI : public BrowserContextKeyedAPI,
                              public ReadingListModelObserver {
  friend class BrowserContextKeyedAPIFactory<ReadingListPrivateAPI>;
  const raw_ptr<Profile> profile_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ReadingListPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

public:
  explicit ReadingListPrivateAPI(content::BrowserContext* context);
  ~ReadingListPrivateAPI() override;
  ReadingListPrivateAPI(const ReadingListPrivateAPI&) = delete;
  ReadingListPrivateAPI& operator=(const ReadingListPrivateAPI&) = delete;

  static void Init();

  static ReadingListPrivateAPI* FromBrowserContext(
    content::BrowserContext* browser_context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ReadingListPrivateAPI>*
  GetFactoryInstance();

  // ReadingListModelObserver
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListDidApplyChanges(ReadingListModel* model) override;

 private:
  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_model_scoped_observation_{this};
};

class ReadingListPrivateAddFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingListPrivate.add",
                             READINGLIST_ADD)

  ReadingListPrivateAddFunction() = default;

 protected:
  ~ReadingListPrivateAddFunction() override = default;

 private:
  ResponseAction Run() override;
};

class ReadingListPrivateRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingListPrivate.remove",
                             READINGLIST_REMOVE)

  ReadingListPrivateRemoveFunction() = default;

 protected:
  ~ReadingListPrivateRemoveFunction() override = default;

 private:
  ResponseAction Run() override;
};

class ReadingListPrivateGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingListPrivate.getAll",
                             READINGLIST_GETALL)

  ReadingListPrivateGetAllFunction() = default;

 protected:
  ~ReadingListPrivateGetAllFunction() override = default;

 private:
  ResponseAction Run() override;
};

class ReadingListPrivateSetReadStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingListPrivate.setReadStatus",
                             READINGLIST_SETREADSTATUS)

  ReadingListPrivateSetReadStatusFunction() = default;

 protected:
  ~ReadingListPrivateSetReadStatusFunction() override = default;

 private:
  ResponseAction Run() override;
};


}  // namespace extension

#endif  // EXTENSIONS_API_READING_LIST_API_H_
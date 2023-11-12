//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
#ifndef EXTENSIONS_API_TRANSLATE_HISTORY_TRANSLATE_HISTORY_API_H_
#define EXTENSIONS_API_TRANSLATE_HISTORY_TRANSLATE_HISTORY_API_H_

#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/translate_history.h"
#include "translate_history/th_model_observer.h"

namespace translate_history {
class TH_Model;
}

namespace extensions {

class TranslateHistoryAPI : public BrowserContextKeyedAPI,
                            public EventRouter::Observer,
                            public translate_history::TH_ModelObserver {
 public:
  explicit TranslateHistoryAPI(content::BrowserContext* context);
  ~TranslateHistoryAPI() override = default;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TranslateHistoryAPI>*
  GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override {}

  // translate_history::TH_ModelObserver
  void TH_ModelElementAdded(translate_history::TH_Model* model,
                            int index) override;
  void TH_ModelElementMoved(translate_history::TH_Model* model,
                            int index) override;
  void TH_ModelElementsRemoved(translate_history::TH_Model* model,
                               const std::vector<std::string>& ids) override;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  friend class BrowserContextKeyedAPIFactory<TranslateHistoryAPI>;

  const raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<translate_history::TH_Model> model_ = nullptr;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TranslateHistoryAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;
};

// Base class for those function classes that need a loaded model to finish
// the task.
class TranslateHistoryFunction : public ExtensionFunction,
                                 public ::translate_history::TH_ModelObserver {
 public:
  TranslateHistoryFunction() = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~TranslateHistoryFunction() override = default;
  virtual ResponseValue RunWithModel(translate_history::TH_Model* model) = 0;

 private:
  // ::translate_history::TH_ModelObserver
  void TH_ModelLoaded(translate_history::TH_Model* model) override;
};

class TranslateHistoryGetFunction : public TranslateHistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("translateHistory.get", TRANSLATEHISTORY_GET)
  TranslateHistoryGetFunction() = default;

 private:
  ~TranslateHistoryGetFunction() override = default;

  // TranslateHistoryFunction
  ResponseValue RunWithModel(translate_history::TH_Model* model) override;
};

class TranslateHistoryAddFunction : public TranslateHistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("translateHistory.add", TRANSLATEHISTORY_ADD)
  TranslateHistoryAddFunction() = default;

 private:
  ~TranslateHistoryAddFunction() override = default;

  // TranslateHistoryFunction
  ResponseValue RunWithModel(translate_history::TH_Model* model) override;
};

class TranslateHistoryRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("translateHistory.remove", TRANSLATEHISTORY_REMOVE)
  TranslateHistoryRemoveFunction() = default;

 private:
  ~TranslateHistoryRemoveFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class TranslateHistoryResetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("translateHistory.reset", TRANSLATEHISTORY_RESET)
  TranslateHistoryResetFunction() = default;

 private:
  ~TranslateHistoryResetFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_TRANSLATE_HISTORY_TRANSLATE_HISTORY_API_H_
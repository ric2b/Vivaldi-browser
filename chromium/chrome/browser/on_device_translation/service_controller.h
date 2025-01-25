// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_

#include "base/no_destructor.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace on_device_translation {
enum class LanguagePackKey;
}  // namespace on_device_translation

// This class is the controller that launches the on-device translation service
// and delegates the functionalities.
// TODO(crbug.com/364795294): This class does not support Android yet.
class OnDeviceTranslationServiceController {
 public:
  OnDeviceTranslationServiceController(
      const OnDeviceTranslationServiceController&) = delete;
  OnDeviceTranslationServiceController& operator=(
      const OnDeviceTranslationServiceController&) = delete;

  static OnDeviceTranslationServiceController* GetInstance();

  // Creates a translator class that implements
  // `on_device_translation::mojom::Translator`, and bind it with the
  // `receiver`.
  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
      base::OnceCallback<void(bool)> callback);

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  void CanTranslate(const std::string& source_lang,
                    const std::string& target_lang,
                    base::OnceCallback<void(bool)> callback);

 private:
  friend base::NoDestructor<OnDeviceTranslationServiceController>;

  OnDeviceTranslationServiceController();
  ~OnDeviceTranslationServiceController();

  // Returns the config for the service.
  on_device_translation::mojom::OnDeviceTranslationServiceConfigPtr GetConfig();

  // Registers the installed language pack components.
  void RegisterInstalledLanguagePackComponent();
  // Maybe triggers the language pack install if the required language packs are
  // not installed.
  void MaybeTriggerLanguagePackInstall(const std::string& source_lang,
                                       const std::string& target_lang);
  // Registers the language pack component.
  void RegisterLanguagePackComponent(on_device_translation::LanguagePackKey);

  // Called when the language pack key pref is changed.
  void OnLanguagePackKeyPrefChanged(const std::string& pref_name);

  // TODO(crbug.com/335374928): implement the error handling for the translation
  // service crash.
  mojo::Remote<on_device_translation::mojom::OnDeviceTranslationService>
      service_remote_;
  // Used to listen for changes on the pref values of language packs.
  PrefChangeRegistrar pref_change_registrar_;
  std::set<on_device_translation::LanguagePackKey> registered_language_packs_;
  const on_device_translation::mojom::OnDeviceTranslationServiceConfigPtr
      config_from_command_line_;
};

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_H_

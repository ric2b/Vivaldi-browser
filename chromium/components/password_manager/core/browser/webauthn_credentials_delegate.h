// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/passkey_credential.h"

namespace password_manager {

// Delegate facilitating communication between the password manager and
// WebAuthn. It is associated with a single frame.
class WebAuthnCredentialsDelegate {
 public:
  using OnPasskeySelectedCallback = base::OnceClosure;
  virtual ~WebAuthnCredentialsDelegate() = default;

  // Launches the normal WebAuthn flow that lets users use their phones or
  // security keys to sign-in.
  virtual void LaunchWebAuthnFlow() = 0;

  // Called when the user selects a passkey from the autofill suggestion list
  // The selected credential must be from the list returned by the last call to
  // GetPasskeys(). |callback| should be invoked when the selected passkey is
  // consumed.
  virtual void SelectPasskey(const std::string& backend_id,
                             OnPasskeySelectedCallback callback) = 0;

  // Returns the list of eligible passkeys to fulfill an ongoing WebAuthn
  // request if one has been received and is active. Returns std::nullopt
  // otherwise.
  virtual const std::optional<std::vector<PasskeyCredential>>& GetPasskeys()
      const = 0;

  // Returns whether a "Use a passkey from a different device" option should
  // be offered.
  virtual bool OfferPasskeysFromAnotherDeviceOption() const = 0;

  // Initiates retrieval of passkeys from the platform authenticator.
  // |callback| is invoked when credentials have been received, which could be
  // immediately.
  virtual void RetrievePasskeys(base::OnceCallback<void()> callback) = 0;

  // Returns true iff a passkey was selected via `SelectPasskey` and
  // `OnPasskeySelectedCallback` has not been called yet.
  virtual bool HasPendingPasskeySelection() = 0;

#if BUILDFLAG(IS_ANDROID)
  // Called to start the hybrid sign-in flow in Play Services.
  virtual void ShowAndroidHybridSignIn() = 0;

  // Returns true if hybrid sign-in is available, and the option should be
  // shown on conditional UI autofill surfaces.
  virtual bool IsAndroidHybridAvailable() const = 0;
#endif

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<WebAuthnCredentialsDelegate> AsWeakPtr() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WEBAUTHN_CREDENTIALS_DELEGATE_H_

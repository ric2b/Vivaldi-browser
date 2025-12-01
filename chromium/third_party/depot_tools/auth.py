# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Google OAuth2 related functions."""

from __future__ import annotations

import collections
import datetime
import functools
import httplib2
import json
import logging
import os
from typing import Optional
from dataclasses import dataclass

import subprocess2

# TODO: Should fix these warnings.
# pylint: disable=line-too-long

# This is what most GAE apps require for authentication.
OAUTH_SCOPE_EMAIL = 'https://www.googleapis.com/auth/userinfo.email'
# Gerrit and Git on *.googlesource.com require this scope.
OAUTH_SCOPE_GERRIT = 'https://www.googleapis.com/auth/gerritcodereview'
# Deprecated. Use OAUTH_SCOPE_EMAIL instead.
OAUTH_SCOPES = OAUTH_SCOPE_EMAIL


@dataclass
class ReAuthContext:
    """Provides contextual information for ReAuth."""
    host: str  # Hostname (e.g. chromium-review.googlesource.com)
    project: str  # Project on host (e.g. chromium/src)

    def to_git_cred_attrs(self) -> bytes:
        """Returns bytes to be used as the input of `git-credentials-luci` in
           exchange for a ReAuth token.
        """
        assert self.project
        return f"""
capability[]=authtype
protocol=https
host={self.host}
path={self.project}
""".lstrip().encode('utf-8')


# Mockable datetime.datetime.utcnow for testing.
def datetime_now():
    return datetime.datetime.utcnow()


# OAuth access token or ID token with its expiration time (UTC datetime or None
# if unknown).
class Token(collections.namedtuple('Token', [
        'token',
        'expires_at',
])):
    def needs_refresh(self):
        """True if this token should be refreshed."""
        if self.expires_at is not None:
            # Allow 30s of clock skew between client and backend.
            return datetime_now() + datetime.timedelta(
                seconds=30) >= self.expires_at
        # Token without expiration time never expires.
        return False


class LoginRequiredError(Exception):
    """Interaction with the user is required to authenticate."""
    def __init__(self, scopes=OAUTH_SCOPE_EMAIL):
        self.scopes = scopes
        msg = ('You are not logged in. Please login first by running:\n'
               '  %s' % self.login_command)
        super(LoginRequiredError, self).__init__(msg)

    @property
    def login_command(self) -> str:
        return 'luci-auth login -scopes "%s"' % self.scopes


class GitLoginRequiredError(Exception):
    """Interaction with the user is required to authenticate.

    This is for git-credential-luci, not luci-auth.
    """

    def __init__(self):
        msg = (
            'You are not logged in to Gerrit. Please login first by running:\n'
            '  %s' % self.login_command)
        super(GitLoginRequiredError, self).__init__(msg)

    @property
    def login_command(self) -> str:
        return 'git credential-luci login'


class GitReAuthRequiredError(Exception):
    """Interaction with the user is required to ReAuth.

    This is for git-credential-luci, not luci-auth.
    """

    def __init__(self):
        msg = ('You have not done ReAuth. Please run and try again:\n'
               '  %s' % self.reauth_command)
        super(GitReAuthRequiredError, self).__init__(msg)

    @property
    def reauth_command(self) -> str:
        return 'git credential-luci reauth'


class GitUnknownError(Exception):
    """Unknown error from git-credential-luci."""

    def __init__(self):
        msg = ('Unknown error from git-credential-luci. Try logging in? Run:\n'
               '  %s' % self.login_command)
        super(GitLoginRequiredError, self).__init__(msg)

    @property
    def login_command(self) -> str:
        return 'git credential-luci login'


def has_luci_context_local_auth():
    """Returns whether LUCI_CONTEXT should be used for ambient authentication."""
    ctx_path = os.environ.get('LUCI_CONTEXT')
    if not ctx_path:
        return False
    try:
        with open(ctx_path) as f:
            loaded = json.load(f)
    except (OSError, IOError, ValueError):
        return False
    return loaded.get('local_auth', {}).get('default_account_id') is not None


class Authenticator(object):
    """Object that knows how to refresh access tokens or id tokens when needed.

    Args:
        scopes: space separated oauth scopes. It's used to generate access tokens.
            Defaults to OAUTH_SCOPE_EMAIL.
        audience: An audience in ID tokens to claim which clients should accept it.
    """
    def __init__(self, scopes=OAUTH_SCOPE_EMAIL, audience=None):
        self._access_token = None
        self._scopes = scopes
        self._id_token = None
        self._audience = audience

    def has_cached_credentials(self):
        """Returns True if credentials can be obtained.

        If returns False, get_access_token() or get_id_token() later will probably
        ask for interactive login by raising LoginRequiredError.

        If returns True, get_access_token() or get_id_token() won't ask for
        interactive login.
        """
        return bool(self._get_luci_auth_token())

    def get_access_token(self):
        """Returns AccessToken, refreshing it if necessary.

        Raises:
            LoginRequiredError if user interaction is required.
        """
        if self._access_token and not self._access_token.needs_refresh():
            return self._access_token

        # Token expired or missing. Maybe some other process already updated it,
        # reload from the cache.
        self._access_token = self._get_luci_auth_token()
        if self._access_token and not self._access_token.needs_refresh():
            return self._access_token

        # Nope, still expired. Needs user interaction.
        logging.debug('Failed to create access token')
        raise LoginRequiredError(self._scopes)

    def get_id_token(self):
        """Returns id token, refreshing it if necessary.

        Returns:
            A Token object.

        Raises:
            LoginRequiredError if user interaction is required.
        """
        if self._id_token and not self._id_token.needs_refresh():
            return self._id_token

        self._id_token = self._get_luci_auth_token(use_id_token=True)
        if self._id_token and not self._id_token.needs_refresh():
            return self._id_token

        # Nope, still expired. Needs user interaction.
        logging.debug('Failed to create id token')
        raise LoginRequiredError()

    def authorize(self, http, use_id_token=False):
        """Monkey patches authentication logic of httplib2.Http instance.

        The modified http.request method will add authentication headers to each
        request.

        Args:
            http: An instance of httplib2.Http.

        Returns:
            A modified instance of http that was passed in.
        """
        # Adapted from oauth2client.OAuth2Credentials.authorize.
        request_orig = http.request

        @functools.wraps(request_orig)
        def new_request(uri,
                        method='GET',
                        body=None,
                        headers=None,
                        redirections=httplib2.DEFAULT_MAX_REDIRECTS,
                        connection_type=None):
            headers = (headers or {}).copy()
            auth_token = self.get_access_token(
            ) if not use_id_token else self.get_id_token()
            headers['Authorization'] = 'Bearer %s' % auth_token.token
            return request_orig(uri, method, body, headers, redirections,
                                connection_type)

        http.request = new_request
        return http

    ## Private methods.

    def _get_luci_auth_token(self, use_id_token=False):
        logging.debug('Running luci-auth token')
        if use_id_token:
            args = ['-use-id-token'] + ['-audience', self._audience
                                        ] if self._audience else []
        else:
            args = ['-scopes', self._scopes]
        try:
            out, err = subprocess2.check_call_out(['luci-auth', 'token'] +
                                                  args + ['-json-output', '-'],
                                                  stdout=subprocess2.PIPE,
                                                  stderr=subprocess2.PIPE)
            logging.debug('luci-auth token stderr:\n%s', err)
            token_info = json.loads(out)
            return Token(
                token_info['token'],
                datetime.datetime.utcfromtimestamp(token_info['expiry']))
        except subprocess2.CalledProcessError as e:
            # subprocess2.CalledProcessError.__str__ nicely formats
            # stdout/stderr.
            logging.error('luci-auth token failed: %s', e)
            return None


class GerritAuthenticator(object):
    """Object that knows how to refresh access tokens for Gerrit.

    Unlike Authenticator, this is specifically for authenticating Gerrit
    requests.
    """

    # Exitcodes for `git-credential-luci`.
    # See: https://chromium.googlesource.com/infra/luci/luci-go/+/main/client/cmd/git-credential-luci/main.go
    _GCL_EXITCODE_SUCCESS = 0
    _GCL_EXITCODE_UNCLASSIFIED = 1
    _GCL_EXITCODE_LOGIN_REQUIRED = 2
    _GCL_EXITCODE_REAUTH_REQUIRED = 3

    def __init__(self):
        self._access_token: Optional[str] = None

    def get_access_token(self) -> str:
        """Returns AccessToken, refreshing it if necessary.

        This token can't satisfy ReAuth requirements. Use
        `get_authorization_header` method instead.

        Raises:
            GitLoginRequiredError: if user login is required.
        """
        logging.debug('Running git-credential-luci')
        env = os.environ.copy()
        env['LUCI_ENABLE_REAUTH'] = '0'
        out_bytes = self._call_helper(['git-credential-luci', 'get'],
                                      stdin=subprocess2.DEVNULL,
                                      stdout=subprocess2.PIPE,
                                      stderr=subprocess2.PIPE,
                                      env=env)
        out = self._parse_creds_helper_out(out_bytes)
        if password := out.get("password", None):
            return password

        logging.error('git-credential-luci did not return a token')
        raise GitUnknownError()

    def get_authorization_header(self, context: ReAuthContext) -> str:
        """Returns an HTTP Authorization header to authenticate requests.

        This method supports ReAuth, but it may be missing ReAuth credentials
        (i.e. RAPT token), if ReAuth isn't required based on the context, or if
        ReAuth support is disabled.

        Raises:
            GitLoginRequiredError: if user login is required.
            GitReAuthRequiredError: if ReAuth is required.
        """
        logging.debug('Running git-credential-luci (with reauth)')
        creds_attrs = context.to_git_cred_attrs()
        logging.debug('git-credential-luci stdin:\n%s', creds_attrs)
        out_bytes = self._call_helper(['git-credential-luci', 'get'],
                                      stdin=creds_attrs,
                                      stdout=subprocess2.PIPE,
                                      stderr=subprocess2.PIPE)
        if header := self._extract_authorization_header(out_bytes):
            return header

        logging.error('git-credential-luci did not return a token')
        raise GitUnknownError()

    def _extract_authorization_header(self, out_bytes: bytes) -> Optional[str]:
        out = self._parse_creds_helper_out(out_bytes)
        # Check for ReAuth token and return it's available.
        authtype = out.get("authtype", None)
        credential = out.get("credential", None)
        if authtype and credential:
            return f"{authtype} {credential}"

        # If the helper returns non-reauth token, it means ReAuth isn't required and
        # the access token already satisfies the request.
        if password := out.get("password", None):
            return f"Bearer {password}"

        # If the helper also didn't return an access token, something is wrong.
        logging.error(
            'git-credential-luci did not return a token or a ReAuth token')
        return None

    def _parse_creds_helper_out(self, out_bytes: str) -> Dict[str, str]:
        """Parse credential helper's output to a dictionary.

        Note, this function doesn't handle arrays (e.g. key[]=value).
        """
        result = {}
        for line in out_bytes.decode().splitlines():
            if '=' in line:
                key, value = line.split('=', 1)
                result[key] = value.strip()
        return result

    def _call_helper(self, args, **kwargs) -> bytes:
        """Calls the helper executable and propagate errors based on exit code.

        Returns output as bytes if successful.
        Raises:
            GitLoginRequiredError
            GitReAuthRequiredError
            GitUnknownError
        """
        stdout_stderr, exitcode = subprocess2.communicate(args, **kwargs)
        stdout, stderr = stdout_stderr
        logging.debug('git-credential-luci stderr:\n%s', stderr)

        if exitcode == self._GCL_EXITCODE_SUCCESS:
            return stdout

        if exitcode == self._GCL_EXITCODE_LOGIN_REQUIRED:
            raise GitLoginRequiredError()
        if exitcode == self._GCL_EXITCODE_REAUTH_REQUIRED:
            raise GitReAuthRequiredError()

        err = subprocess2.CalledProcessError(exitcode, args, kwargs.get('cwd'),
                                             stdout, stderr)
        logging.error('git-credential-luci failed: %s', err)
        raise err

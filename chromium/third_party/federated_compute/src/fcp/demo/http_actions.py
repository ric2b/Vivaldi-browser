# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Utilities for creating proto service and HTTP action handlers.

The `@proto_action` function annotates a method as implementing a proto service
method. The annotated method should have the type
`Callable[[RequestMessage], ResponseMessage]`. The decorator will take care of
transcoding to/from a HTTP request, similar to
https://cloud.google.com/endpoints/docs/grpc/transcoding. The transcoding only
supports proto-over-http ('?alt=proto').

The `@http_action` function annotates a method as implementing a HTTP action at
some request path. The annotated method will receive the request body, and
should return a `HttpResponse`.

The `create_handler` function merges one or more objects with decorated methods
into a single request handler that's compatible with `http.server`.
"""

import collections
import dataclasses
import enum
import gzip
import http
import http.server
import re
from typing import Any, Callable, Mapping, Match, Pattern, Type, TypeVar
import urllib.parse
import zlib

from absl import logging

from google.api import annotations_pb2
from google.protobuf import descriptor_pool
from google.protobuf import message
from google.protobuf import message_factory

_CallableT = TypeVar('_CallableT', bound=Callable)

_HTTP_ACTION_ATTR = '_http_action_data'
_POOL = descriptor_pool.Default()


@dataclasses.dataclass(frozen=True)
class HttpError(Exception):
  """An Exception specifying the HTTP error to return."""
  code: http.HTTPStatus


@dataclasses.dataclass(frozen=True)
class HttpResponse:
  """Information for a successful HTTP response."""
  body: bytes
  headers: Mapping[str, str] = dataclasses.field(default_factory=lambda: {})


def proto_action(*,
                 service=str,
                 method=str) -> Callable[[_CallableT], _CallableT]:
  """Decorator annotating a method as handling a proto service method.

  The `google.api.http` annotation on the method will determine what requests
  will be handled by the decorated function. Only a subset of methods and path
  patterns are currently supported.

  The decorated method will be called with the request message; it should return
  a response message or or throw an `HttpError`.

  Args:
    service: The full name of the proto service.
    method: The name of the method.

  Returns:
    An annotated function.
  """
  try:
    desc = _POOL.FindServiceByName(service).FindMethodByName(method)
  except KeyError as e:
    raise ValueError(f'Unable to find /{service}.{method}.') from e

  rule = desc.GetOptions().Extensions[annotations_pb2.http]
  pattern_kind = rule.WhichOneof('pattern')
  try:
    http_method = _HttpMethod[pattern_kind.upper()]
  except KeyError as e:
    raise ValueError(
        f'The google.api.http annotation on /{service}.{method} is invalid '
        'or unsupported.') from e
  path = _convert_pattern(getattr(rule, pattern_kind), alt_proto=True)

  def handler(match: Match[str], body: bytes,
              fn: Callable[[message.Message], message.Message]) -> HttpResponse:
    if hasattr(message_factory, 'GetMessageClass'):
      request = message_factory.GetMessageClass(desc.input_type)()
    else:
      request = message_factory.MessageFactory().GetPrototype(desc.input_type)()
    if rule.body == '*':
      try:
        request.ParseFromString(body)
      except message.DecodeError as e:
        raise HttpError(code=http.HTTPStatus.BAD_REQUEST) from e
    elif rule.body:
      setattr(request, rule.body, body)
    # Set any fields from the request path.
    for prop, value in match.groupdict().items():
      try:
        unescaped = urllib.parse.unquote(value)
      except UnicodeError as e:
        raise HttpError(code=http.HTTPStatus.BAD_REQUEST) from e
      setattr(request, prop, unescaped)

    response_body = fn(request).SerializeToString()
    return HttpResponse(
        body=response_body,
        headers={
            'Content-Length': len(response_body),
            'Content-Type': 'application/x-protobuf'
        })

  def annotate_method(func: _CallableT) -> _CallableT:
    setattr(func, _HTTP_ACTION_ATTR,
            _HttpActionData(method=http_method, path=path, handler=handler))
    return func

  return annotate_method


def http_action(*, method: str,
                pattern: str) -> Callable[[_CallableT], _CallableT]:
  """Decorator annotating a method as an HTTP action handler.

  Request matching the method and pattern will be handled by the decorated
  method. The pattern may contain bracket-enclosed keywords (e.g.,
  '/data/{path}'), which will be matched against the request and passed
  to the decorated function as keyword arguments.

  The decorated method will be called with the request body (if any) and any
  keyword args from the path pattern; it should return a `HttpResponse` or throw
  an `HttpError`.

  Args:
    method: The type of HTTP method ('GET' or 'POST').
    pattern: The url pattern to match.

  Returns:
    An annotated function.
  """
  try:
    http_method = _HttpMethod[method.upper()]
  except KeyError as e:
    raise ValueError(f'unsupported HTTP method `{method}`') from e
  path = _convert_pattern(pattern)

  def handler(match: Match[str], body: bytes,
              fn: Callable[[bytes], HttpResponse]) -> HttpResponse:
    try:
      args = {k: urllib.parse.unquote(v) for k, v in match.groupdict().items()}
    except UnicodeError as e:
      raise HttpError(code=http.HTTPStatus.BAD_REQUEST) from e
    return fn(body, **args)

  def annotate_method(func: _CallableT) -> _CallableT:
    setattr(func, _HTTP_ACTION_ATTR,
            _HttpActionData(method=http_method, path=path, handler=handler))
    return func

  return annotate_method


def create_handler(*services: Any) -> Type[http.server.BaseHTTPRequestHandler]:
  """Builds a BaseHTTPRequestHandler that delegates to decorated methods.

  The returned BaseHTTPRequestHandler class will route requests to decorated
  methods of the provided services, or return 404 if the request path does not
  match any action handlers. If the request path matches multiple registered
  action handlers, it's unspecified which will be invoked.

  Args:
    *services: A list of objects with methods decorated with `@proto_action` or
      `@http_action`.

  Returns:
    A BaseHTTPRequestHandler subclass.
  """

  # Collect all handlers, keyed by HTTP method.
  handlers = collections.defaultdict(lambda: [])
  for service in services:
    for attr_name in dir(service):
      attr = getattr(service, attr_name)
      if not callable(attr):
        continue
      data = getattr(attr, _HTTP_ACTION_ATTR, None)
      if isinstance(data, _HttpActionData):
        handlers[data.method].append((data, attr))

  format_handlers = lambda h: ''.join([f'\n  * {e[0].path.pattern}' for e in h])
  logging.debug(
      'Creating HTTP request handler for path patterns:\nGET:%s\nPOST:%s',
      format_handlers(handlers[_HttpMethod.GET]),
      format_handlers(handlers[_HttpMethod.POST]))

  class RequestHandler(http.server.BaseHTTPRequestHandler):
    """Handler that delegates to `handlers`."""

    def do_GET(self) -> None:  # pylint:disable=invalid-name (override)
      self._handle_request(_HttpMethod.GET, read_body=False)

    def do_POST(self) -> None:  # pylint:disable=invalid-name (override)
      self._handle_request(_HttpMethod.POST)

    def _handle_request(self,
                        method: _HttpMethod,
                        read_body: bool = True) -> None:
      """Reads and delegates an incoming request to a registered handler."""
      for data, fn in handlers[method]:
        match = data.path.fullmatch(self.path)
        if match is None:
          continue

        try:
          body = self._read_body() if read_body else b''
          response = data.handler(match, body, fn)
        except HttpError as e:
          logging.debug('%s error: %s', self.path, e)
          return self.send_error(e.code)
        return self._send_response(response)

      # If no handler matched the path, return an error.
      self.send_error(http.HTTPStatus.NOT_FOUND)

    def _read_body(self) -> bytes:
      """Reads the body of the request."""
      body = self.rfile.read(int(self.headers['Content-Length']))
      if self.headers['Content-Encoding'] == 'gzip':
        try:
          body = gzip.decompress(body)
        except (gzip.BadGzipFile, zlib.error) as e:
          raise HttpError(http.HTTPStatus.BAD_REQUEST) from e
      elif self.headers['Content-Encoding']:
        logging.warning('Unsupported content encoding %s',
                        self.headers['Content-Encoding'])
        raise HttpError(http.HTTPStatus.BAD_REQUEST)
      return body

    def _send_response(self, response: HttpResponse) -> None:
      """Sends a successful response message."""
      self.send_response(http.HTTPStatus.OK)
      for keyword, value in response.headers.items():
        self.send_header(keyword, value)
      self.end_headers()
      self.wfile.write(response.body)

  return RequestHandler


class _HttpMethod(enum.Enum):
  GET = 1
  POST = 2


@dataclasses.dataclass(frozen=True)
class _HttpActionData:
  """Data tracked for HTTP actions.

  Attributes:
    method: The name of the HTTP method to handle.
    path: Requests matching this pattern will be handled.
    handler: The handler function, which receives the path match, request body,
      and decorated function.
  """
  method: _HttpMethod
  path: Pattern[str]
  handler: Callable[[Match[str], bytes, Callable[..., Any]], HttpResponse]


def _convert_pattern(pattern: str, alt_proto=False) -> Pattern[str]:
  """Converts a Google API pattern to a regexp with named groups."""
  # Subfields are not supported and will generate a regexp compilation error.
  pattern_regexp = re.sub(r'\\\{(.+?)\\\}', r'(?P<\1>[^/?]*)',
                          re.escape(pattern))
  if alt_proto:
    pattern_regexp += r'\?%24alt=proto'
  try:
    return re.compile(pattern_regexp)
  except re.error as e:
    raise ValueError(f'unable to convert `{pattern}` to a regexp') from e

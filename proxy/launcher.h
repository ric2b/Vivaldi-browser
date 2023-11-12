//
// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.
//
#ifndef PROXY_LAUNCHER_H_
#define PROXY_LAUNCHER_H_

#include <string>

namespace vivaldi {
namespace proxy {

struct ConnectSettings {
  ConnectSettings();
  ~ConnectSettings();

  std::string local_port;
  std::string remote_host;
  std::string remote_port;
  std::string token;
  std::string user_password;
};

struct ConnectState {
  int pid;
  std::string message;
};

bool connect(const ConnectSettings& settings, ConnectState& state);
void disconnect();

} // proxy
} // vivaldi

#endif // PROXY_LAUNCHER_H_
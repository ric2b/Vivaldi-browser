// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include <map>
#include <string>

template <typename NodeContent>
class HostNameTrieNode {
 public:
  using Edges = std::map<char, size_t>;
  HostNameTrieNode();
  ~HostNameTrieNode();

  AddEdge(char next_char, size_t next_node);

 private:
  Edges edges_;
  NodeContent content_;
};

template <typename NodeContent>
class HostNameTrie {
 public:
  HostNameTrie();
  ~HostNameTrie();

  AddHostname(const std::string& hostname, NodeContent node_content);

 private:
  std::vector<HostNameTrieNode>
};

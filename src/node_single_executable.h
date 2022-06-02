#ifndef SRC_NODE_SINGLE_BINARY_H_
#define SRC_NODE_SINGLE_BINARY_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstddef>
#include <string>

namespace node {

namespace single_executable {

struct single_executable_info {
  bool valid = false;

  int argc;
  char** argv;

  size_t script_pos;
  std::string script;
};

void initialize();

}  // namespace single_executable

namespace per_process {

extern single_executable::single_executable_info single_executable_info;

}  // namespace per_process

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SINGLE_BINARY_H_

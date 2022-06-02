#ifndef SRC_NODE_SINGLE_BINARY_H_
#define SRC_NODE_SINGLE_BINARY_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstddef>

namespace node {
namespace single_binary {

struct single_binary_info {
  bool valid;

  int argc;
  char** argv;

  size_t script_pos;
  char* script;
};

single_binary_info* checkForSingleBinary();

}  // namespace single_binary
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SINGLE_BINARY_H_

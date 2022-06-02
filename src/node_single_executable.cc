#include "node_single_executable.h"

#include <wordexp.h>
#include <cstring>
#include <exception>
#include <fstream>
#include <string>
#include <string_view>

#include "env-inl.h"

namespace node {

using v8::Context;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

namespace per_process {

single_executable::single_executable_info single_executable_info;

}

namespace single_executable {

#define FUSE_SENTINEL "dL7pKGdnNz796PbbjQWNKmHXBZaB9tsX"
#define FUSE_VERSION "\001"
#define FUSE_LENGTH "\001"

#define FUSE_DEFAULT_SINGLE_EXECUTABLE "\000"

static volatile const char* fuse =
    FUSE_SENTINEL FUSE_VERSION FUSE_LENGTH FUSE_DEFAULT_SINGLE_EXECUTABLE;

enum fuse_position {
  single_executable = sizeof(FUSE_SENTINEL) + 1,
} fuse_wire_position;

static constexpr std::string_view MAGIC_HEADER = "JSCODE";
static constexpr std::string_view VERSION_CHARS = "00000001";
static constexpr std::string_view FLAG_CHARS = "00000000";

std::string executable_path() {
  char exec_path_buf[2 * PATH_MAX];
  size_t exec_path_len = sizeof(exec_path_buf);

  if (uv_exepath(exec_path_buf, &exec_path_len) == 0) {
    return std::string(exec_path_buf, exec_path_len);
  }

  return "";
}

std::ifstream* search_single_executable_data() {
  auto exec = executable_path();
  if (exec.empty()) {
    return nullptr;
  }

  auto f = new std::ifstream(exec);
  if (!f->is_open() || !f->good() || f->eof()) {
    delete f;
    return nullptr;
  }

  std::string needle;
  needle += MAGIC_HEADER;
  needle += VERSION_CHARS;
  needle += FLAG_CHARS;

  constexpr auto buf_size = 1 << 20;

  auto buf = new char[buf_size];
  auto buf_view = std::string_view(buf, buf_size);
  auto buf_pos = buf_view.npos;

  size_t f_pos = 0;

  // first read
  f->read(buf, buf_size);
  f_pos += f->gcount();
  buf_pos = buf_view.find(needle);
  if (buf_pos != buf_view.npos) {
    f_pos = f_pos - f->gcount() + buf_pos;

    f->clear();
    f->seekg(f_pos, std::ios::beg);

    delete[] buf;
    return f;
  }

  // subsequent reads, moving window
  while (!f->eof()) {
    std::memcpy(buf, buf + buf_size - needle.size(), needle.size());
    f->read(buf + needle.size(), buf_size - needle.size());
    f_pos += f->gcount();
    buf_pos = buf_view.find(needle);
    if (buf_pos != buf_view.npos) {
      f_pos = f_pos - f->gcount() - needle.size() + buf_pos;

      f->clear();
      f->seekg(f_pos, std::ios::beg);

      delete[] buf;
      return f;
    }
  }

  delete[] buf;
  delete f;
  return nullptr;
}

#if __has_cpp_attribute(clang::optnone)
[[clang::optnone]]
#endif
#if __has_cpp_attribute(gnu::noinline)
[[gnu::noinline]]
#endif
bool check_fuse(fuse_position position) {
  return fuse[position] == 1;
}

void initialize() {
  if (!check_fuse(fuse_position::single_executable)) {
    return;
  }

  auto singleBinaryData = search_single_executable_data();
  if (singleBinaryData == nullptr) {
    return;
  }

  per_process::single_executable_info.valid = true;

  auto f_pos = singleBinaryData->tellg();

  // 4096 chars should be more than enough to deal with
  // header + node options + script size
  // but definitely not elegant to have this limit
  constexpr auto buf_size = 1 << 12;

  char buf[buf_size];

  singleBinaryData->read(buf, buf_size);

  // parse node options
  wordexp_t parsedArgs;
  wordexp(buf + MAGIC_HEADER.size() + VERSION_CHARS.size() + FLAG_CHARS.size(),
          &parsedArgs,
          WRDE_NOCMD);

  per_process::single_executable_info.argc = 0;
  while (parsedArgs.we_wordv[per_process::single_executable_info.argc] !=
         nullptr) {
    per_process::single_executable_info.argc++;
  }

  per_process::single_executable_info.argv = parsedArgs.we_wordv;

  // parse script size and position
  auto buf_view = std::string_view(buf, buf_size);
  auto buf_pos = buf_view.npos;
  auto buf_pos2 = buf_pos;

  buf_pos = buf_view.find('\0');
  if (buf_pos == buf_view.npos || ++buf_pos > buf_size - 1) {
    std::terminate();
  }

  buf_pos2 = buf_view.find('\0', buf_pos);
  if (buf_pos2 == buf_view.npos || ++buf_pos2 > buf_size - 1) {
    std::terminate();
  }

  per_process::single_executable_info.script_pos = buf_pos2 + f_pos;
  per_process::single_executable_info.script.resize(atol(buf + buf_pos));

  singleBinaryData->clear();
  singleBinaryData->seekg(per_process::single_executable_info.script_pos,
                          std::ios::beg);
  singleBinaryData->read(per_process::single_executable_info.script.data(),
                         per_process::single_executable_info.script.size());

  if (singleBinaryData->gcount() !=
      per_process::single_executable_info.script.size()) {
    per_process::single_executable_info.valid = false;
  }

  delete singleBinaryData;
}

}  // namespace single_executable
}  // namespace node

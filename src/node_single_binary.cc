#include "node_single_binary.h"

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

namespace single_binary {

static single_binary_info* info = nullptr;

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

std::ifstream* search_single_binary_data() {
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

struct single_binary_info* checkForSingleBinary() {
  if (info != nullptr) {
    return info;
  }

  info = new single_binary_info;
  info->valid = false;

  auto singleBinaryData = search_single_binary_data();

  if (singleBinaryData != nullptr) {
    info->valid = true;

    auto f_pos = singleBinaryData->tellg();

    // 4096 chars should be more than enough to deal with
    // header + node options + script size
    // but definitely not elegant to have this limit
    constexpr auto buf_size = 1 << 12;

    char buf[buf_size];

    singleBinaryData->read(buf, buf_size);

    // parse node options
    wordexp_t parsedArgs;
    wordexp(
        buf + MAGIC_HEADER.size() + VERSION_CHARS.size() + FLAG_CHARS.size(),
        &parsedArgs,
        WRDE_NOCMD);

    info->argc = 0;
    while (parsedArgs.we_wordv[info->argc] != nullptr) {
      info->argc++;
    }

    info->argv = parsedArgs.we_wordv;

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

    info->script_pos = buf_pos2 + f_pos;

    auto script_size = atol(buf + buf_pos);
    info->script = new char[script_size + 1];

    singleBinaryData->clear();
    singleBinaryData->seekg(info->script_pos, std::ios::beg);
    singleBinaryData->read(info->script, script_size);

    if (singleBinaryData->gcount() != script_size) {
      delete[] info->script;
      info->script = nullptr;
      info->valid = false;
    } else {
      info->script[script_size] = '\0';
    }

    delete singleBinaryData;
  }

  return info;
}

}  // namespace single_binary
}  // namespace node

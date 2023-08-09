#include <iostream>
#include <stdexcept>

// waio
#include "winaio.h"

namespace {

class iocp_awaiter {
 public:
  iocp_awaiter(OVERLAPPED* o) noexcept : o_(o){};
  bool await_ready() const noexcept { return false; }
  void await_resume() const noexcept {}
  void await_suspend(std::coroutine_handle<> resume) noexcept {
    o_->Pointer = resume.address();
  }

 private:
  OVERLAPPED* o_;
};

}  // namespace

namespace waio {

thread_local loop waio::loop::instance;

waio::loop::loop() noexcept {
  iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
}

void waio::loop::run() noexcept {
  for (;;) {
    DWORD tbytes;
    ULONG_PTR ckey;
    LPOVERLAPPED o;
    GetQueuedCompletionStatus(iocp, &tbytes, &ckey, &o, INFINITE);
    void* addr;
    if (o == &o_) {
      break;
    } else if (o != nullptr) {
      addr = o->Pointer;
    } else {
      addr = reinterpret_cast<void*>(ckey);
    }
    auto resume = std::coroutine_handle<>::from_address(addr);
    resume();
  }
}

void waio::loop::exit() noexcept {
  PostQueuedCompletionStatus(iocp, 0, 0, &o_);
}

HANDLE waio::create(const std::filesystem::path& path) noexcept {
  auto pipe = CreateNamedPipeA(
      path.string().c_str(),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
      std::numeric_limits<uint16_t>::max(),
      std::numeric_limits<uint16_t>::max(), 0, nullptr);
  CreateIoCompletionPort(pipe, waio::loop::instance.iocp, 0, 1);
  return pipe;
}

HANDLE open(const std::filesystem::path& path) noexcept {
  auto stream =
      CreateFileA(path.string().c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                  nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
  CreateIoCompletionPort(stream, waio::loop::instance.iocp, 0, 1);
  return stream;
}

task<void> waio::connect(HANDLE pipe) {
  OVERLAPPED o{};
  ConnectNamedPipe(pipe, &o);
  co_await iocp_awaiter{&o};
}

task<std::string> read(HANDLE stream, unsigned size) {
  std::string result;
  result.resize(size);
  OVERLAPPED o{};
  size_t offset = 0;
  while (offset < size) {
    ReadFile(stream, result.data() + offset, static_cast<DWORD>(size - offset),
             nullptr, &o);
    co_await iocp_awaiter{&o};
    offset += o.InternalHigh;
  }
  co_return result;
}

task<void> waio::write(HANDLE stream, std::string data) {
  OVERLAPPED o{};
  size_t offset = 0;
  while (offset < data.size()) {
    WriteFile(stream, data.data() + offset,
              static_cast<DWORD>(data.size() - offset), nullptr, &o);
    co_await iocp_awaiter{&o};
    offset += o.InternalHigh;
  }
}

}  // namespace waio
#include <iostream>
#include <stdexcept>

// waio
#include "winaio.h"

namespace {

class iocp_awaiter {
 public:
  iocp_awaiter(HANDLE handle) noexcept : handle_(handle){};
  bool await_ready() const noexcept { return false; }
  void await_resume() const noexcept {}
  void await_suspend(std::coroutine_handle<> resume) noexcept {
    CreateIoCompletionPort(handle_, waio::loop::instance.iocp,
                           reinterpret_cast<ULONG_PTR>(resume.address()), 1);
  }

 private:
  HANDLE handle_;
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
    auto addr = reinterpret_cast<void*>(ckey);
    auto resume = std::coroutine_handle<>::from_address(addr);
    resume();
  }
}

HANDLE waio::create(const std::filesystem::path& path) noexcept {
  return CreateNamedPipeA(
      path.string().c_str(),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
      std::numeric_limits<uint16_t>::max(),
      std::numeric_limits<uint16_t>::max(), 0, nullptr);
}

HANDLE open(const std::filesystem::path& path) noexcept {
  return CreateFileA(path.string().c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                     nullptr, OPEN_EXISTING, 0, nullptr);
}

task<void> waio::connect(HANDLE pipe) {
  OVERLAPPED io{};
  ConnectNamedPipe(pipe, &io);
  iocp_awaiter awaitable{pipe};
  co_await awaitable;
}

}  // namespace waio
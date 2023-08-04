#include "winaio.h"

#include <Windows.h>

namespace waio {

struct loop::impl {};

loop::loop() noexcept : impl_(std::make_unique<loop::impl>()) {}
void loop::run() noexcept {
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
  }
}

thread_local single_threaded_loop single_threaded_loop::instance;

}  // namespace waio
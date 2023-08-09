#include <iostream>
#include <semaphore>
#include <stdexcept>
#include <thread>

// winaio header files
#include "winaio.h"

std::binary_semaphore signal_prod_ready{0};
const std::filesystem::path pipe_name = "\\\\.\\pipe\\waio_test_pipe";

void producer() {
  auto pipe = waio::create(pipe_name);
  if (pipe == INVALID_HANDLE_VALUE) throw std::runtime_error{"bad pipe handle"};
  auto async = [pipe]() -> waio::fire_and_forget {
    auto defer = waio::connect(pipe);
    signal_prod_ready.release();
    co_await defer;
  };
  async();
  waio::loop::instance.run();
}
void consumer() {
  signal_prod_ready.acquire();
  auto pipe = waio::open(pipe_name);
  std::string buffer;
  buffer.resize(100);
  DWORD read;
  ReadFile(pipe, buffer.data(), 100, &read, nullptr);
  waio::loop::instance.run();
}

int main() {
  std::jthread threads[] = {std::jthread{producer}, std::jthread{consumer}};
}

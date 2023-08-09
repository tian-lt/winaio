#include <iostream>
#include <mutex>
#include <semaphore>
#include <stdexcept>
#include <string_view>
#include <thread>

// winaio header files
#include "winaio.h"

constexpr std::string_view test_msg = "hello pipe";
std::binary_semaphore signal_prod_ready{0};
const std::filesystem::path pipe_name = "\\\\.\\pipe\\waio_test_pipe";
std::mutex stdio_mtx;

void producer() {
  auto pipe = waio::create(pipe_name);
  if (pipe == INVALID_HANDLE_VALUE) throw std::runtime_error{"bad pipe handle"};
  auto async = [pipe]() -> waio::fire_and_forget {
    {
      auto defer = waio::connect(pipe);
      signal_prod_ready.release();
      co_await defer;
    }
    co_await waio::write(pipe, std::string{test_msg});
    {
      std::scoped_lock lck{stdio_mtx};
      std::cout << "producer says: " << test_msg << std::endl;
    }
    waio::loop::instance.exit();
  };
  async();
  waio::loop::instance.run();
}

void consumer() {
  signal_prod_ready.acquire();
  auto pipe = waio::open(pipe_name);
  if (pipe == INVALID_HANDLE_VALUE) throw std::runtime_error{"bad pipe handle"};
  auto async = [pipe]() -> waio::fire_and_forget {
    auto resp =
        co_await waio::read(pipe, static_cast<unsigned>(test_msg.size()));
    {
      std::scoped_lock lck{stdio_mtx};
      std::cout << "consumer gets: " << resp << std::endl;
    }
    waio::loop::instance.exit();
  };
  async();
  waio::loop::instance.run();
}

int main() {
  std::jthread threads[] = {std::jthread{producer}, std::jthread{consumer}};
}

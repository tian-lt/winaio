#include <thread>

// clang-format off
// keep std header files ahead
// clang-format on

#include <Windows.h>

#include "winaio.h"

void producer() { waio::single_threaded_loop::instance.run(); }
void consumer() { waio::single_threaded_loop::instance.run(); }

int main() {
  std::jthread threads[] = {std::jthread{producer}, std::jthread{consumer}};
}

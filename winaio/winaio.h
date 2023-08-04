#ifndef WINAIO_H
#define WINAIO_H

#include <memory>

namespace waio {

class loop {
 public:
  loop() noexcept;
  void run() noexcept;

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

class single_threaded_loop : public loop {
 public:
  static thread_local single_threaded_loop instance;
};

}  // namespace waio

#endif  // !WINAIO_H

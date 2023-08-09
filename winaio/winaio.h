#ifndef WINAIO_H
#define WINAIO_H

#include <coroutine>
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <type_traits>

// windows header files
#define NOMINMAX
#include <Windows.h>

namespace waio::details {

inline void check_bool(bool val) {
  if (!val) {
    throw std::runtime_error{"unexpected false."};
  }
}

inline void check_handle(HANDLE handle) {
  if (handle == INVALID_HANDLE_VALUE) {
    throw std::runtime_error{"invalid handle."};
  }
}

}  // namespace waio::details

namespace waio {

struct loop {
  loop() noexcept;
  void run() noexcept;
  static thread_local loop instance;
  HANDLE iocp = INVALID_HANDLE_VALUE;
};

struct fire_and_forget {};

template <class... Args>
struct std::coroutine_traits<waio::fire_and_forget, Args...> {
  struct promise_type {
    waio::fire_and_forget get_return_object() const noexcept { return {}; }
    void return_void() const noexcept {}
    auto initial_suspend() const noexcept { return std::suspend_never{}; }
    auto final_suspend() const noexcept { return std::suspend_never{}; }
    void unhandled_exception() const noexcept { std::terminate(); }
  };
};

template <class T>
class task_completion_source;

template <class T>
class task {
  friend class task_completion_source<T>;

 public:
  void schedule(std::coroutine_handle<> resume) noexcept {
    source_->schedule(resume);
  }

 private:
  task_completion_source<T>* source_;
};

template <class T>
  requires(!std::is_void_v<T> && !std::is_reference_v<T>)
class task_completion_source<T> {
 public:
  task<T> get_task() noexcept {
    task<T> task;
    task.source_ = this;
    return task;
  }
  void schedule(std::coroutine_handle<> resume) noexcept { resume_ = resume; }

  template <class U>
  void set_value(U&& value) noexcept {
    value_ = std::forward<U>(value);
    details::check_bool(PostQueuedCompletionStatus(
        loop::instance.iocp, 0, reinterpret_cast<ULONG_PTR>(resume_.address()),
        nullptr));
  }
  T& get() const noexcept { return value_; }

 private:
  std::coroutine_handle<> resume_;
  T value_;
};

template <>
class task_completion_source<void> {
 public:
  task<void> get_task() noexcept {
    task<void> task;
    task.source_ = this;
    return task;
  }
  void schedule(std::coroutine_handle<> resume) noexcept { resume_ = resume; }
  void set_value() const noexcept {
    details::check_bool(PostQueuedCompletionStatus(
        loop::instance.iocp, 0, reinterpret_cast<ULONG_PTR>(resume_.address()),
        nullptr));
  }
  void get() const noexcept {}

 private:
  std::coroutine_handle<> resume_;
};

template <class T, class... Args>
  requires(!std::is_void_v<T> && !std::is_reference_v<T>)
struct std::coroutine_traits<waio::task<T>, Args...> {
  struct promise_type : waio::task_completion_source<T> {
    waio::task<T> get_return_object() noexcept { return this->get_task(); }
    auto initial_suspend() const noexcept { return std::suspend_never{}; }
    auto final_suspend() const noexcept { return std::suspend_never{}; }
    void unhandled_exception() const noexcept { std::terminate(); }
    template <class U>
    void return_value(U&& val) noexcept {
      this->set_value(std::forward<U>(val));
    }
  };
};

template <class... Args>
struct std::coroutine_traits<waio::task<void>, Args...> {
  struct promise_type : waio::task_completion_source<void> {
    waio::task<void> get_return_object() noexcept { return this->get_task(); }
    auto initial_suspend() const noexcept { return std::suspend_never{}; }
    auto final_suspend() const noexcept { return std::suspend_never{}; }
    void unhandled_exception() const noexcept { std::terminate(); }
    void return_void() const noexcept { this->set_value(); }
  };
};

inline auto operator co_await(waio::task<void> task) noexcept {
  struct awaiter : waio::task<void> {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> resume) noexcept {
      this->schedule(resume);
    }
    void await_resume() const noexcept {}
  };
  return awaiter{std::move(task)};
}

HANDLE create(const std::filesystem::path& path) noexcept;
HANDLE open(const std::filesystem::path& path) noexcept;
task<void> connect(HANDLE pipe);

}  // namespace waio

#endif  // !WINAIO_H

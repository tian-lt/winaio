#ifndef WINAIO_H
#define WINAIO_H

#include <coroutine>
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
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

class loop {
 public:
  loop() noexcept;
  void run() noexcept;
  void exit() noexcept;
  static thread_local loop instance;
  HANDLE iocp = INVALID_HANDLE_VALUE;

 private:
  OVERLAPPED o_ = {};
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
template <>
class task_completion_source<void>;

template <class T>
class task {
  friend class task_completion_source<T>;

 public:
  void schedule(std::coroutine_handle<> resume) noexcept {
    source_->schedule(resume, this);
  }
  void set_value(T&& value) { value_ = std::move(value); }

 protected:
  T value_;

 private:
  task_completion_source<T>* source_;
};

template <>
class task<void> {
  friend class task_completion_source<void>;

 public:
  void schedule(std::coroutine_handle<> resume) noexcept;

 private:
  task_completion_source<void>* source_;
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
  void schedule(std::coroutine_handle<> resume, task<T>* task) noexcept {
    resume_ = resume;
    task_ = task;
  }
  void set_value(T&& value) noexcept {
    task_->set_value(std::move(value));
    details::check_bool(PostQueuedCompletionStatus(
        loop::instance.iocp, 0, reinterpret_cast<ULONG_PTR>(resume_.address()),
        nullptr));
  }

 private:
  std::coroutine_handle<> resume_;
  task<T>* task_;
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

 private:
  std::coroutine_handle<> resume_;
};

inline void task<void>::schedule(std::coroutine_handle<> resume) noexcept {
  source_->schedule(resume);
}

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

template <class T>
  requires(!std::is_void_v<T> && !std::is_reference_v<T>)
inline auto operator co_await(waio::task<T> task) noexcept {
  struct awaiter : waio::task<T> {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> resume) noexcept {
      this->schedule(resume);
    }
    T await_resume() noexcept { return T{std::move(this->value_)}; }
  };
  return awaiter{std::move(task)};
}

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
task<std::string> read(HANDLE stream, unsigned size);
task<void> write(HANDLE stream, std::string data);

}  // namespace waio

#endif  // !WINAIO_H

#ifndef DOSSIER_THREAD_POOL_H
#define DOSSIER_THREAD_POOL_H

#include <deque>
#include <vector>
#include <future>

#include "thread_t.h"

namespace util {
/*
 * Allow threads to execute unhindered
 * while keeping full controll over the threads.
 */
template<class T>
class ThreadPool {
public:
  typedef T __return;
  typedef std::packaged_task<__return() > __task;
  typedef std::future<__return> __future;
private:
  std::deque<__task> _task;

  std::vector<thread_t> _thread;
  std::mutex _task_mutex;

  bool _continue;
public:

  ThreadPool(int threads) : _thread(threads), _continue(true) {
    for (auto & t : _thread) {
      t = thread_t(&ThreadPool::_main, this);
    }
  }

  ~ThreadPool() {
    join();
  }

  __future push(__task && newTask) {
    __future future = newTask.get_future();

    std::lock_guard<std::mutex> lg(_task_mutex);
    _task.emplace_back(std::move(newTask));

    return future;
  }

  void join() {
    if (!_continue) return;

    _continue = false;
    for (auto & t : _thread) {
      t.join();
    }
  }
private:

  int _pop(__task &task) {
    std::lock_guard<std::mutex> lg(_task_mutex);

    if (_task.empty()) {
      return -1;
    }

    task = std::move(_task.front());
    _task.pop_front();
    return 0;
  }
public:

  void _main() {
    std::chrono::milliseconds sleeper(10);

    __task task;
    while (_continue) {
      if (_pop(task)) {
        std::this_thread::sleep_for(sleeper);
        continue;
      }
      task();
    }

    // Execute remaining tasks
    while (!_pop(task)) {
      task();
    }
  }
};

}
#endif

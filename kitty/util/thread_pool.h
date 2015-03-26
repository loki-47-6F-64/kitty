#ifndef KITTY_THREAD_POOL_H
#define KITTY_THREAD_POOL_H

#include <kitty/util/task_pool.h>
#include <kitty/util/thread_t.h>

namespace util {
/*
 * Allow threads to execute unhindered
 * while keeping full controll over the threads.
 */
class ThreadPool : public TaskPool {
public:
  typedef TaskPool::__task __task;
  
private:
  std::vector<thread_t> _thread;

  std::condition_variable _cv;
  std::mutex _lock;
  
  std::atomic<bool> _continue;
public:

  ThreadPool(int threads) : _thread(threads), _continue(true) {
    for (auto & t : _thread) {
      t = thread_t(&ThreadPool::_main, this);
    }
  }

  ~ThreadPool() {
    join();
  }

  template<class Function>
  auto push(Function && newTask) -> std::future<decltype(newTask())> {
    auto future = TaskPool::push(std::forward<Function>(newTask));
    
    _cv.notify_one();
    return future;
  }
  
  template<class Function>
  auto push(Function && newTask, int64_t milli) -> std::future<decltype(newTask())> {
    auto future = TaskPool::push(std::forward<Function>(newTask), milli);
    
    _cv.notify_one();
    return future;
  }
  
  void join() {
    if (!_continue.exchange(false)) return;
    
    _cv.notify_all();
    for (auto & t : _thread) {
      t.join();
    }
  }

public:

  void _main() {
    while (_continue.load()) {
      if(auto task = this->pop()) {
        (*task)->run();
      }
      else {
        std::unique_lock<std::mutex> uniq_lock(_lock);
        _cv.wait(uniq_lock);
      }
    }

    // Execute remaining tasks
    while(auto task = this->pop()) {
      (*task)->run();
    }
  }
};

}
#endif

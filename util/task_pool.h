#ifndef KITTY_TASK_POOL_H
#define KITTY_TASK_POOL_H

#include <deque>
#include <vector>
#include <future>
#include <chrono>
#include <utility>

#include <kitty/util/optional.h>
#include <kitty/util/utility.h>
namespace util {

class TaskPool {
  class _ImplBase {
  public:
    inline virtual ~_ImplBase() = default;
    
    virtual void run() = 0;
  };
  
  template<class Function>
  class _Impl : public TaskPool::_ImplBase {
    Function _func;
    
  public:
    
    _Impl(Function&& f) : _func(std::forward<Function>(f)) { }
    
    void run() {
      _func();
    }
  };

public:  
  typedef std::unique_ptr<_ImplBase> __task;
  
  typedef std::chrono::steady_clock::time_point __time_point;
private:
  std::deque<__task> _tasks;
  std::vector<std::pair<__time_point, __task>> _timer_tasks; 
  std::mutex _task_mutex;

public:
  template<class Function>
  auto push(Function && newTask) -> std::future<decltype(newTask())> {
    typedef decltype(newTask()) __return;
    typedef std::packaged_task<__return()> task_t;
    
    task_t task(std::move(newTask));
    
    auto future = task.get_future();
    
    std::lock_guard<std::mutex> lg(_task_mutex);
    _tasks.emplace_back(toRunnable(std::move(task)));
    
    return future;
  }
  
  template<class Function>
  auto push(Function &&newTask, int64_t milli) -> std::future<decltype(newTask())> {
    typedef decltype(newTask()) __return;
    typedef std::packaged_task<__return()> task_t;
    
    __time_point time_point = std::chrono::steady_clock::now() + std::chrono::milliseconds(milli);
    
    task_t task(std::move(newTask));
    
    auto future = task.get_future();
    
    std::lock_guard<std::mutex> lg(_task_mutex);
    
    auto it = _timer_tasks.cbegin();
    for(; it < _timer_tasks.cend(); ++it) {
      if(std::get<0>(*it) >= time_point) {
        break;
      }
    }
    _timer_tasks.emplace(it, time_point, toRunnable(std::move(task)));
    
    return future;
  }
  
  util::Optional<__task> pop() {
    
    std::lock_guard<std::mutex> lg(_task_mutex);   
    
    if(!_tasks.empty()) {
      __task task = std::move(_tasks.front());
      _tasks.pop_front();
      return task;
    }
    
    if(!_timer_tasks.empty() && std::get<0>(_timer_tasks.back()) <= std::chrono::steady_clock::now()) {
      __task task = std::move(std::get<1>(_timer_tasks.back()));
      _timer_tasks.pop_back();
      
      return task;
    }
    
    return util::Optional<__task>();
  }
private:
  
  template<class Function>
  std::unique_ptr<_ImplBase> toRunnable(Function &&f) {
    return util::mk_uniq<_Impl<Function>>(std::move(f));
  }
};
}
#endif
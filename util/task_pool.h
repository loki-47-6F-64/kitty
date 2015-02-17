#ifndef KITTY_TASK_POOL_H
#define KITTY_TASK_POOL_H

#include <deque>
#include <vector>
#include <future>
#include <chrono>
#include <utility>

#include <kitty/util/optional.h>

namespace util {
template<class T>
class TaskPool {
public:
  typedef T __return;
  typedef std::packaged_task<__return()> __task;
  typedef std::future<__return> __future;
  
  typedef std::chrono::steady_clock::time_point __time_point;
private:
  std::deque<__task> _tasks;
  std::vector<std::pair<__time_point, __task>> _timer_tasks; 
  std::mutex _task_mutex;
  
  bool _continue;
public:
  template<class Function>
  __future push(Function && newTask) {
    __task task(std::move(newTask));
    
    __future future = task.get_future();
    
    std::lock_guard<std::mutex> lg(_task_mutex);
    _tasks.emplace_back(std::move(task));
    
    return future;
  }
  
  template<class Function>
  __future push(Function &&newTask, int64_t milli) {
    __time_point time_point = std::chrono::steady_clock::now() + std::chrono::milliseconds(milli);
    
    __task task(std::move(newTask));
    
    __future future = task.get_future();
    
    std::lock_guard<std::mutex> lg(_task_mutex);
    
    auto it = _timer_tasks.cbegin();
    for(; it < _timer_tasks.cend(); ++it) {
      if(std::get<0>(*it) >= time_point) {
        break;
      }
    }
    _timer_tasks.emplace(it, time_point, std::move(task));
    
    return future;
  }
  
  util::Optional<__task> pop() {
    
    std::lock_guard<std::mutex> lg(_task_mutex);
    if(!_timer_tasks.empty() && std::get<0>(_timer_tasks.back()) <= std::chrono::steady_clock::now()) {
      __task task = std::move(std::get<1>(_timer_tasks.back()));
      _timer_tasks.pop_back();
      
      return task;
    }
    
    
    if(!_tasks.empty()) {
      __task task = std::move(_tasks.front());
      _tasks.pop_front();
      return task;
    }
    
    return util::Optional<__task>();
  }
};
}
#endif
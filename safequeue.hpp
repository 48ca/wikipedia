#ifndef SAFEQUEUE_H

#define SAFEQUEUE_H

#include <deque>
#include <mutex>
#include <functional>
#include <atomic>

#include "logging.hpp"

template<typename T>
class SafeQueue {
  private:
    std::deque<T> queue;
    std::mutex queue_mutex;

    std::mutex cond_mutex;
    std::condition_variable cond;
    unsigned id;

  public:
    SafeQueue(unsigned id) {
        this->id = id;
        log("Starting queue " + std::to_string(id));
    };
    ~SafeQueue() {};

    void push(T item) {
        std::lock_guard<std::mutex> lk(queue_mutex);
        queue.emplace_back(item);
        cond.notify_one();
    };

    T wait_for_element() {
        T front;
        std::unique_lock<std::mutex> lk(cond_mutex);
        cond.wait(lk, [this, &front]{
            std::lock_guard<std::mutex> inner_lock(queue_mutex);
            if(queue.size() > 0) {
                front = queue.front();
                queue.pop_front();
                return true;
            } else {
                return false;
            }
        });
        return front;
    };

};

#endif // SAFEQUEUE_H

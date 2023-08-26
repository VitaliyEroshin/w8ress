#pragma once

#include <concurrency/scheduler/executor.hpp>
#include <mutex>
#include <queue>
#include <condition_variable>

class MPMCQueue {
public:
    bool Push(concurrency::ITask* task) {
        std::unique_lock lock(mutex);

        queue.push(task);
        condvar.notify_one();

        return true;
    }

    concurrency::ITask* Pop() {
        std::unique_lock lock(mutex);

        if (queue.empty()) {
            condvar.wait(lock);
        }

        if (!running.load()) {
            return nullptr;
        }

        auto* t = queue.back();
        queue.pop();

        return t;
    }

    void Stop() {
        std::unique_lock lock(mutex);

        running.store(false);
        condvar.notify_all();
    }

private:
    std::queue<concurrency::ITask*> queue;
    std::mutex mutex; // Gonna be lock-free, but for now mutex, sorry
    std::condition_variable condvar;
    std::atomic<bool> running{true};
};
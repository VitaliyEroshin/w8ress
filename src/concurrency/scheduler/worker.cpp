#include <concurrency/scheduler/worker.hpp>

using namespace concurrency;

Worker::Worker(Scheduler* scheduler) 
        : scheduler(scheduler)
{}

void Worker::Start() {
    thread.emplace(&Worker::Run, this);
}

void Worker::Join() {
    if ((*thread).joinable()) {
        (*thread).join();
    }
}

void Worker::Run() {
    while (true) {
        ITask* task = scheduler->global_queue.Pop();
        
        if (task == nullptr) {
            return;
        }

        (*task)();
    }
}
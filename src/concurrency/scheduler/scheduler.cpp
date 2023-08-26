#include <concurrency/scheduler/scheduler.hpp>

using namespace concurrency;

void Scheduler::Stop() {
    global_queue.Stop();
    for (auto& worker : workers) {
        worker.Join();
    }
}

bool Scheduler::Submit(concurrency::ITask *task) {
    global_queue.Push(task);
    return true;
}

Scheduler::Scheduler(size_t num_workers) : num_workers(num_workers) {
    for (size_t i = 0; i < num_workers; ++i) {
        workers.emplace_back(this);
    }

    for (size_t i = 0; i < num_workers; ++i) {
        workers[i].Start();
    }
}
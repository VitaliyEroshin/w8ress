#pragma once

#include <concurrency/scheduler/executor.hpp>
#include <concurrency/scheduler/worker.hpp>
#include <concurrency/support/mpmc_queue.hpp>
#include <thread>
#include <vector>

namespace concurrency {

class Worker;

class Scheduler : public concurrency::IExecutor {
    friend class Worker;
public:
    bool Submit(concurrency::ITask* task) override;
    
    void Stop() override;

    Scheduler(size_t num_workers = std::thread::hardware_concurrency());

private:
    size_t num_workers;
    std::vector<Worker> workers;
    MPMCQueue global_queue;
};

}; // namespace concurrency
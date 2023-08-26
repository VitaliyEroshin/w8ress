#pragma once
#include <concurrency/scheduler/scheduler.hpp>
#include <thread>
#include <optional>

namespace concurrency {

class Scheduler;

class Worker {
public:
    Worker(Scheduler* scheduler);

    void Join();

    void Start();

private:
    void Run();

    Scheduler* scheduler;
    std::optional<std::thread> thread;
};

}; // namespace concurrency
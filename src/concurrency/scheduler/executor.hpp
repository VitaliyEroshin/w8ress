#pragma once

#include <iostream>
#include <memory>
#include <new>
#include <type_traits>

namespace concurrency {

struct ITask {
    virtual void operator()() = 0;

    virtual ~ITask() = default;
};

class IExecutor {
public:
    // Submits intrusive task to executor
    virtual bool Submit(ITask* task) = 0;

    // Waits for executor to stop
    virtual void Stop() = 0;

    virtual ~IExecutor() = default;
};

class InlineExecutor : public IExecutor {
public:
    bool Submit(ITask* task) override {
        std::cout << "Submitted task!" << std::endl;
        (*task)();
        return true;
    }

    void Stop() override {
        return;
    }
};

template <typename Function, typename Allocator = void /* A little trick to make allocator to self */> 
struct Task : public ITask {
    using Alloc = std::conditional_t<std::is_same<Allocator, void>::value,
                                   std::allocator<Task>, Allocator>;

    using AllocTraits = std::allocator_traits<Alloc>;

    static Task *MakeTask(Function f, Alloc allocator = Alloc()) {
        

        Task *task = AllocTraits::allocate(allocator, 1);
        AllocTraits::construct(allocator, task, std::move(f), std::move(allocator));
        return task;
    }

    void operator()() override {
        f();
        AllocTraits::destroy(alloc, this);
        AllocTraits::deallocate(alloc, this, 1);
    }


    Task(Function f, Alloc alloc): f(std::move(f)), alloc(std::move(alloc)) {}

private:

    Function f;
    Alloc alloc;
};

}; // namespace concurrency
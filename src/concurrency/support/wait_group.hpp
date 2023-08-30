#include <atomic>
#include <cstddef>
namespace concurrency {

class WaitGroup {
public:
    WaitGroup(size_t count): counter(count) {}

    void Wait() {
        size_t remaining = counter.load();
        while (remaining != 0) {
            counter.wait(remaining);
            remaining = counter.load();
        }
    }

    void Pass(uint32_t count = 1) {
        counter.fetch_sub(count);
        counter.notify_all();
    }

private:
    std::atomic<uint32_t> counter;
};

}; // namespace concurrency
#include "concurrency/scheduler/worker.hpp"
#include <csignal>
#include <cstring>
#include <io/server.hpp>
#include <signal.h>
#include <iostream>

concurrency::IExecutor* executor;

ServerSettings settings;
Server server(settings);

class SignalHandler {
public:
    static void SigtermHandler() {
        std::cout << "\nSIGTERM received... Bye ^-^" << std::endl;
        server.Terminate();
        std::cout << "Server terminated!" << std::endl;
        executor->Stop();
        std::cout << "Executor stopped!" << std::endl;
    }

    static void SetSignalHandler(int signum, auto* handler, struct sigaction* action) {
        memset(action, 0, sizeof(struct sigaction));
        action->sa_handler = reinterpret_cast<decltype(action->sa_handler)>(handler);
        action->sa_flags = SA_RESTART;
        sigaction(signum, action, NULL);
    }

    void SetupSignals() {
        SetSignalHandler(SIGTERM, SigtermHandler, &sigterm_action);
        SetSignalHandler(SIGINT, SigtermHandler, &sigint_action);
    }

private:
    struct sigaction sigterm_action;
    struct sigaction sigint_action;
};

int main() {
    executor = new concurrency::Scheduler();

    SignalHandler signal_handler;
    signal_handler.SetupSignals();

    server.SetExecutor(executor);
    server.Listen();

    // delete executor;
}
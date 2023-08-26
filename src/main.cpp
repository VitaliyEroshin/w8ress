#include <csignal>
#include <cstring>
#include <io/server.hpp>
#include <signal.h>
#include <iostream>

Server server(ServerSettings{});

class SignalHandler {
public:
    static void SigtermHandler() {
        std::cout << "\nSIGTERM received... Bye ^-^" << std::endl;
        server.Terminate();
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
    SignalHandler signal_handler;
    signal_handler.SetupSignals();

    server.Listen();
}
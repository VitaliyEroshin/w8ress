#include <bits/types/sig_atomic_t.h>
#include <csignal>
#include <optional>
#include <set>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>

struct ServerSettings {
    /* Protocol ports. std::nullopt if you want to disable specific protocol */
    std::optional<int> http_port = 80;
    std::optional<int> https_port = 443;

    std::string static_content_location = "content/";
    std::string not_found_placeholder = "404.html";
    std::string index_page = "index.html";
};

class HttpServer {
public:
    void ApplySettings(ServerSettings);
    void Setup(int port);
    void CheckEvents();
    void Terminate();

    ~HttpServer();

private:
    using EpollEvent = epoll_event;

    struct EpollEventData {
        int fd;
    };

    void ProcessEpollEvent(EpollEvent* data);
    void AcceptNewConnection();
    void ReadSocket(int fd);
    void DisconnectSocket(int fd);
    void ParseHttpRequest(std::string data, int fd);
    std::string ParseFilesystemPath(std::string path);

    volatile sig_atomic_t running{true};
    int socket;
    int epoll;
    EpollEvent epoll_events[SOMAXCONN];
    EpollEventData epoll_data[SOMAXCONN];
    ServerSettings settings;
};

class Server {
public:
    Server(ServerSettings);
    void Listen();
    void Terminate();

private:
    void SetupHttpListener(int port);

    volatile std::sig_atomic_t running{true};
    ServerSettings settings;
    HttpServer http;
};
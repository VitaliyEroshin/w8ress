#include <optional>
#include <set>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>

struct ServerSettings {
    /* Protocol ports. std::nullopt if you want to disable specific protocol */
    std::optional<int> http_port = 80;
    std::optional<int> https_port = 443;
};

class HttpServer {
public:
    void Setup(int port);

private:
    using EpollEvent = epoll_event;

    struct EpollEventData {
        int fd;
    };

    void ProcessEpollEvent(EpollEvent* data);
    void AcceptNewConnection();
    void ReadSocket(int fd);
    void DisconnectSocket(int fd);
    void ParseHttpRequest(std::string data);

    int socket;
    int epoll;
    EpollEvent epoll_events[SOMAXCONN];
    EpollEventData epoll_data[SOMAXCONN];
};

class Server {
public:
    Server(ServerSettings);
    void Listen();

private:
    void SetupHttpListener(int port);

    ServerSettings settings;
    HttpServer http;
};
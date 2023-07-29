#include <optional>
#include <set>
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
    struct Connection {
        epoll_event event;
    };

    struct EpollEventData {
        int fd;
        Connection* connection = nullptr;
    };

    void ProcessEpollEvent(EpollEventData data);
    void AcceptNewConnection();

    int socket;
    int epoll;
    EpollEventData epoll_data[SOMAXCONN];
    Connection connections[SOMAXCONN];
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
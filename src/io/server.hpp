#include <optional>

struct ServerSettings {
    /* Protocol ports. std::nullopt if you want to disable specific protocol */
    std::optional<int> http_port = 80;
    std::optional<int> https_port = 443;
};

class HttpServer {
public:
    HttpServer();
    void Setup(int port);

private:
    int socket;
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
#include <io/server.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>

Server::Server(ServerSettings settings): settings(std::move(settings)) {
    if (settings.http_port) {
        http.Setup(settings.http_port.value());
    }
}

namespace detail {

int MakeSocket(int port) {
    int socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
}

}; // namespace detail

void HttpServer::Setup(int port) {
    socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    // TODO: More comfortable error handling needs to be here
    if (bind(socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind error");
        return;
    }

    if (listen(socket, SOMAXCONN) < 0) {
        perror("Listen error");
        return -1;
    }

    return sock;
}
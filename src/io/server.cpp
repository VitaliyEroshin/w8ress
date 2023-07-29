#include <arpa/inet.h>
#include <cstdio>
#include <fcntl.h>
#include <io/server.hpp>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>

Server::Server(ServerSettings settings): settings(std::move(settings)) {
    if (settings.http_port) {
        http.Setup(settings.http_port.value());
    }
}

namespace detail {

int MakeSocket(int port) {
    int socket = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    // TODO: More comfortable error handling needs to be here
    if (bind(socket, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind error");
        return -1;
    }

    if (listen(socket, SOMAXCONN) < 0) {
        perror("Listen error");
        return -1;
    }

    return socket;
}

}; // namespace detail

void HttpServer::Setup(int port) {
    socket = detail::MakeSocket(port);

    epoll = epoll_create1(0);
    epoll_event listen_event;
    EpollEventData listen_data = {socket};

    listen_event.data.ptr = &listen_data;
    listen_event.events = EPOLLIN;

    epoll_ctl(epoll, EPOLL_CTL_ADD, socket, &listen_event);

    const size_t max_events = SOMAXCONN;
    epoll_event events[max_events];

    std::cout << "Socket fd: " << socket << std::endl;

    while (true) {
        size_t trigerred = epoll_wait(epoll, events, max_events, -1);

        std::cout << "Triggered " << trigerred << " events" << std::endl;

        for (size_t i = 0; i < trigerred; ++i) {
            EpollEventData* data = reinterpret_cast<EpollEventData*>(events[i].data.ptr);

            ProcessEpollEvent(*data);
        }
    }

    shutdown(socket, SHUT_RDWR);
}

void HttpServer::ProcessEpollEvent(EpollEventData data) {
    if (data.fd == socket) {
        AcceptNewConnection();
    }
    std::cout << data.fd << std::endl;
    return;
}

void HttpServer::AcceptNewConnection() {
    int fd = accept(socket, nullptr, nullptr);

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    auto& con = connections[fd];
    auto& data = epoll_data[fd];

    con.event.data.ptr = &data;
    con.event.data.fd = fd;
    data.fd = fd;
    data.connection = &con;
    con.event.events = EPOLLIN;

    epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &con.event);
    return;
}

#include <arpa/inet.h>
#include <cstdio>
#include <fcntl.h>
#include <io/server.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Server::Server(ServerSettings settings): settings(std::move(settings)) {
    if (settings.http_port) {
        http.Setup(settings.http_port.value());
    }
}

void Server::Listen() {
    while (running) {
        if (settings.http_port) {
            http.CheckEvents();
        }
    }
}

void Server::Terminate() {
    http.Terminate();
    running = false;
}

void HttpServer::Terminate() {
    running = false;
}

HttpServer::~HttpServer() {
    shutdown(socket, SHUT_RDWR);
    close(socket);
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

struct HttpResponse {
    std::string proto;
    int status_code;
    std::string reason_phrase;
    std::string content_type;
    std::optional<int> content_length;
    std::string content;

    std::string GetResponse() {
        std::string response;

        response += proto + " ";
        response += std::to_string(status_code) + " ";
        response += reason_phrase + "\n";
        if (!content_type.empty()) {
            response += "Content-Type: " + content_type + "\n";
        }

        if (content_length) {
            response += "Content-Length: " + std::to_string(content_length.value()) + "\n";
        }

        response += "\n";
        
        if (!content.empty()) {
            response += content;
        }

        return response;
    }
};

void ExecuteScript(int client, const char* path) {
    pid_t child_pid = fork();

    if (child_pid == 0) {
        dup2(client, 1);
        execlp(path, path, NULL);
        perror("execlp failed");
        exit(1);
    }

    waitpid(child_pid, NULL, 0);
}

void RedirectFileContent(int client, int desc, int length) {
    const int max_buffer_size = 4096;
    char buffer[max_buffer_size];

    char* content = mmap(NULL, length, PROT_READ, MAP_PRIVATE, desc, 0);

    for (int i = 0; i < length;) {
        int to_copy =
            (length - i < max_buffer_size ? length - i : max_buffer_size);
        memcpy(buffer, content + i, to_copy);
        write(client, buffer, to_copy);
        i += to_copy;
    }

    munmap(content, length);
}

void Respond(HttpResponse response, int fd) {
    std::string data = response.GetResponse();
    write(fd, data.c_str(), data.length());
}

void ResolveStaticContent(std::string path, int fd) {
    HttpResponse response;
    response.proto = "HTTP/1.1";

    int desc = open(path.c_str(), O_RDONLY);
    if (desc < 0) {
        if (errno == ENOENT) {
            response.status_code = 404;
            response.reason_phrase = "Not Found";
            response.content_length = 0;

            Respond(std::move(response), fd);
            return;
        }

        if (errno == EACCES) {
            response.status_code = 403;
            response.reason_phrase = "Forbidden";
            response.content_length = 0;

            Respond(std::move(response), fd);
            return;
        }

        return;
    }

    struct stat file_stat;

    fstat(desc, &file_stat);
    int length = file_stat.st_size;

    response.status_code = 200;
    response.reason_phrase = "OK";

    if ((S_IXUSR & file_stat.st_mode) != 0) {
        Respond(std::move(response), fd);

        ExecuteScript(fd, path.c_str());
    } else {
        response.content_length = length;
        Respond(std::move(response), fd);

        RedirectFileContent(fd, desc, length);
    }

    close(desc);
}

}; // namespace detail

void HttpServer::Setup(int port) {
    socket = detail::MakeSocket(port);

    epoll = epoll_create1(0);
    
    epoll_data[socket] = {socket};
    epoll_events[socket].data.ptr = &epoll_data[socket];
    epoll_events[socket].events = EPOLLIN;

    epoll_ctl(epoll, EPOLL_CTL_ADD, socket, &epoll_events[socket]);

    std::cout << "HTTP server is up and ready UwU! Go grab some cup of coffee" << std::endl;
}

void HttpServer::CheckEvents() {
    const size_t max_events = SOMAXCONN;

    // Threadlocal?
    static epoll_event events[max_events];

    ssize_t triggered = epoll_wait(epoll, events, max_events, -1);

    if (triggered < 0 && running) {
        perror("Epoll error");
    }

    for (ssize_t i = 0; i < triggered; ++i) {
        ProcessEpollEvent(&events[i]);
    }
}

void HttpServer::ProcessEpollEvent(epoll_event* event) {
    EpollEventData* data = reinterpret_cast<EpollEventData*>(event->data.ptr);

    if ((event->events & EPOLLHUP) != 0) {
        DisconnectSocket(data->fd);
        return;
    }
    
    if (data->fd == socket) {
        AcceptNewConnection();
        return;
    }

    ReadSocket(data->fd);
    return;
}

void HttpServer::AcceptNewConnection() {
    int fd = accept(socket, nullptr, nullptr);

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    epoll_data[fd].fd = fd;
    epoll_events[fd].data.ptr = &epoll_data[fd];
    epoll_events[fd].events = EPOLLIN | EPOLLRDHUP;

    epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &epoll_events[fd]);
    return;
}

void HttpServer::ReadSocket(int fd) {
    // Need to be threadlocal here? Though everything should be thread-safe.
    
    const size_t buffer_size = 4096;
    char buff[buffer_size];

    // Some flags?
    int bytes_read = recv(fd, buff, buffer_size, 0);

    if (bytes_read == 0) {
        DisconnectSocket(fd);
        return;
    }

    if (bytes_read < 0) {
        // TODO: Gentle error handling
        DisconnectSocket(fd);
        return;
    }

    std::string buffer(buff);
    ParseHttpRequest(std::move(buffer));
}

void HttpServer::DisconnectSocket(int fd) {
    std::cout <<  "Disconnection on " << fd << " socket" << std::endl;
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

void HttpServer::ParseHttpRequest(std::string data) {
    std::string copy = data;

    std::stringstream ss(std::move(data));

    std::string request;
    ss >> request;

    if (request != "GET") {
        std::cout << "Unknown request type " << request << std::endl;
        std::cout << copy;
        return;
    }

    std::string path;
    ss >> path;

    std::string proto;
    ss >> proto;
    std::cout << "Proto: " << proto << std::endl;
    std::cout << "Requested path: " << path << std::endl;
    path = ParseFilesystemPath(std::move(path));
}

void HttpServer::ParseFilesystemPath(std::string path) {
    return path;
}
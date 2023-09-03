#include <algorithm>
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
#include <sys/wait.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/stat.h>
#include <vector>
#include <io/cache.hpp>

thread_local Cache cached_storage(1024 * 1024 * 128);

Server::Server(ServerSettings settings): settings(settings) {
    if (settings.http_port) {
        http.ApplySettings(settings);
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

void Server::SetExecutor(concurrency::IExecutor* executor) {
    settings.executor = executor;
    http.ApplySettings(settings);
}

void Server::ApplySettings(ServerSettings s) {
    settings = s;
    http.ApplySettings(s);
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
        exit(1);
    }

    if (listen(socket, SOMAXCONN) < 0) {
        perror("Listen error");
        exit(1);
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

void ExecuteScript(int /*client*/, const char* /*path*/) {
    /* Need to put error handling here before uncomment! */

    // pid_t child_pid = fork();

    // if (child_pid == 0) {
    //     dup2(client, 1);
    //     execlp(path, path, NULL);
    //     perror("execlp failed");
    //     exit(1);
    // }

    // waitpid(child_pid, NULL, 0);
}

void RedirectFileContent(int fd, const char* content, int length) {
    // Avoid copying here
    const int max_buffer_size = 4096;
    char buffer[max_buffer_size];

    for (int i = 0; i < length;) {
        int to_copy =
            (length - i < max_buffer_size ? length - i : max_buffer_size);
        memcpy(buffer, content + i, to_copy);
        write(fd, buffer, to_copy);
        i += to_copy;
    }
}

void Respond(HttpResponse response, int fd) {
    std::string data = response.GetResponse();
    write(fd, data.c_str(), data.length());
}

std::string ResolveMimeType(std::string path) {
    if (path.empty()) {
        return "application/octet-stream";
    }

    std::string type;
    auto it = --path.end();
    while (it != path.begin() && *it != '.') {
        type.push_back(*it);
        --it;
    }
    std::reverse(type.begin(), type.end());

    if (type == "html") {
        return "text/html";
    }

    if (type == "css") {
        return "text/css";
    }

    if (type == "wasm") {
        return "application/wasm";
    }

    if (type == "js") {
        return "text/javascript";
    }

    if (type == "ts") {
        return "application/x-typescript";
    }

    if (type == "png" || type == "jpeg" || type == "gif") {
        return "image/" + type;
    }
    
    return "";
}

std::optional<Cache::File> ReadFile(std::string path, Cache& cache) {
    int desc = open(path.c_str(), O_RDONLY);

    if (desc < 0) {
        return std::nullopt;
    }

    struct stat file_stat;
    fstat(desc, &file_stat);

    if (!S_ISREG(file_stat.st_mode)) {
        return std::nullopt;
    }
    

    return cache.GetFileContent(std::move(path));
}

std::string SubstituteTemplate(Cache::File f, std::vector<std::string> args) {
    // Very unsafe, temporary solution

    int balance = 0;
    bool is_arg = false;
    std::string argname;
    std::string result;

    auto it = args.begin();

    for (size_t i = 0; i < f.length; ++i) {
        if (f.content[i] == '{') {
            result.push_back('{');
            if (balance < 0) {
                balance = 0;
            }

            ++balance;
            if (balance == 3) {
                result.pop_back();
                result.pop_back();
                result.pop_back();
                is_arg = true;
            }

            continue;
        }

        if (f.content[i] == '}') {
            result.push_back('}');
            if (balance > 0) {
                balance = 0;
            }

            --balance;
            if (balance != -3) {
                continue;
            }

            result.pop_back();
            result.pop_back();
            result.pop_back();

            is_arg = false;
            argname.clear(); // TODO: Get arg from the map by name
            result += *it;
            ++it;

            continue;
        }
        balance = 0;

        if (is_arg) {
            argname.push_back(f.content[i]);
            continue;
        }

        if (!is_arg) {
            result.push_back(f.content[i]);
            continue;
        }
    }
    return result;
}

void RespondNotFound(int fd) {
    HttpResponse response;
    response.proto = "HTTP/1.1";

    response.status_code = 404;
    response.reason_phrase = "Not Found";
    response.content_length = 0;

    Respond(std::move(response), fd);
}

void RespondOk(int fd, int content_length, std::string mime_type = "text/plain") {
    HttpResponse response;
    response.proto = "HTTP/1.1";

    response.status_code = 200;
    response.reason_phrase = "OK";
    response.content_length = content_length;
    response.content_type = mime_type;

    Respond(std::move(response), fd);
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
        // RedirectFileContent(fd, desc, length, path);
    }

    close(desc);
}

}; // namespace detail

void HttpServer::ApplySettings(ServerSettings s) {
    settings = s;
}

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
        exit(1);
    }

    if (triggered < 0) {
        triggered = 0;
    }

    concurrency::WaitGroup wg(triggered);

    for (ssize_t i = 0; i < triggered; ++i) {
        ProcessEpollEvent(&events[i], wg);
    }

    wg.Wait();
}

void HttpServer::ProcessEpollEvent(epoll_event* event, concurrency::WaitGroup& wg) {
    EpollEventData* data = reinterpret_cast<EpollEventData*>(event->data.ptr);

    if ((event->events & EPOLLHUP) != 0) {
        DisconnectSocket(data->fd);
        wg.Pass();
        return;
    }
    
    if (data->fd == socket) {
        AcceptNewConnection();
        wg.Pass();
        return;
    }

    int fd = data->fd;

    auto lambda = [this, fd, &wg](){
        ReadSocket(fd);
        wg.Pass();
    };

    settings.executor->Submit(
        concurrency::Task<decltype(lambda)>::MakeTask(std::move(lambda))
    );
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
    ParseHttpRequest(std::move(buffer), fd);
}

void HttpServer::DisconnectSocket(int fd) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

void HttpServer::ParseHttpRequest(std::string data, int fd) {
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

    auto response = settings.logic->Get(path);
    response.result = ParseFilesystemPath(std::move(response.result));
    std::string mime_type = detail::ResolveMimeType(response.result);
    
    if (response.type == Logic::Response::HtmlTemplate || response.type == Logic::Response::File) {
        auto f = detail::ReadFile(response.result, cached_storage);

        // Handle std::optional here
        if (!f.has_value()) {
            detail::RespondNotFound(fd);
            return;
        }

        if (response.type == Logic::Response::File) {
            detail::RespondOk(fd, f->length, mime_type);
            detail::RedirectFileContent(fd, f->content, f->length);
            return;
        }

        auto content = detail::SubstituteTemplate(std::move(*f), std::move(response.args));

        detail::RespondOk(fd, content.length(), mime_type);
        detail::RedirectFileContent(fd, content.c_str(), content.length());
        return;
    }

    detail::RespondNotFound(fd);
}

namespace detail {

bool isAllowedCharacter(char c) {
    if (c >= 'a' && c <= 'z') {
        return true;
    }

    if (c >= 'A' && c <= 'Z') {
        return true;
    }

    if (c >= '0' && c <= '9') {
        return true;
    }

    if (c == '/' || c == '.' || c == '-' || c == '_') {
        return true;
    }

    return false;
}

bool hasInvalidCharacters(const std::string& path) {
    for (auto c : path) {
        if (!isAllowedCharacter(c)) {
            return true;
        }
    }

    return false;
}

std::string simplifyPath(std::string path) {
    std::vector<std::string> simplified;
    std::string part = "";

    for (auto c : path) {
        if (c == '/') {
            if (part.empty()) {
                continue;
            }

            if (part == ".") {
                continue;
            }

            if (part == "..") {
                if (simplified.empty()) {
                    continue;
                }

                simplified.pop_back();
            }

            simplified.push_back(std::move(part));
            continue;
        }

        part.push_back(c);
    }
    if (!part.empty()) {
        simplified.push_back(std::move(part));
    }

    std::string simplified_path = "";
    for (auto s : simplified) {
        if (!simplified_path.empty()) {
            simplified_path += "/";
        }
        simplified_path += std::move(s);
    }

    return simplified_path;
}

}; // namespace detail

std::string HttpServer::ParseFilesystemPath(std::string path) {
    if (detail::hasInvalidCharacters(path)) {
        return settings.static_content_location + settings.not_found_placeholder;
    }

    path = detail::simplifyPath(std::move(path));
    return settings.static_content_location + path;
}
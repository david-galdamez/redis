#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unordered_map>
#include <sys/epoll.h>
#include <arpa/inet.h>

#define MAX_EVENTS 1024

struct Conn {
    int fd;

    char r_buffer[4096];
    size_t r_len = 0;

    char w_buffer[4096];
    size_t w_len = 0;
    size_t w_pos = 0;

    bool closing = false;
};

void accept_clients(int server_fd, int epoll_fd) {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);

        int conn_fd = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
        if (conn_fd < 0) {
            break;
        }

        Conn *new_conn = new Conn();
        new_conn->fd = conn_fd;

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.ptr = new_conn;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
            std::cerr << "Epoll ctl failed\n";
        }
    }
}

bool handle_read(Conn *conn) {
    ssize_t n = read(conn->fd, conn->r_buffer + conn->r_len, sizeof(conn->r_buffer) - conn->r_len);
    if (n <= 0) {
        conn->closing = true;
        return false;
    }

    conn->r_len += n;

    std::string request(conn->r_buffer, conn->r_len);
    if (request.find("PING") != std::string::npos) {
        const char* response = "+PONG\r\n";
        memcpy(conn->w_buffer, response, strlen(response));
        conn->w_len = strlen(response);
        conn->w_pos = 0;
    }

    conn->r_len = 0;
    return true;
}

bool handle_write(Conn *conn) {
    while (conn->w_pos < conn->w_len) {
        const ssize_t n = write(conn->fd, conn->w_buffer + conn->w_pos, conn->w_len - conn->w_pos);

        if (n <= 0) {
            conn->closing = true;
            return false;
        }

        conn->w_pos += n;
    }

    conn->w_len = 0;
    conn->w_pos = 0;
    return true;
}

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    const int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(6379);

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 6379\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    std::cout << "Waiting for a client to connect...\n";

    //server event
    epoll_event ev{}, events[MAX_EVENTS]; //events notification

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        std::cerr << "Epoll creation failed\n";
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        std::cerr << "Epoll ctl failed\n";
        return 1;
    }

    std::unordered_map<int, Conn *> connections;

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            std::cerr << "Epoll wait failed\n";
            return 1;
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.ptr == nullptr) {
                accept_clients(server_fd, epoll_fd);
                continue;
            }

            auto conn = static_cast<Conn*>(events[n].data.ptr);

            if (events[n].events & (EPOLLHUP | EPOLLERR)) {
                conn->closing = true;
            }

            if (events[n].events & EPOLLIN) {
                handle_read(conn);
            }

            if (events[n].events & EPOLLOUT) {
                handle_write(conn);
            }

            epoll_event new_ev{};
            new_ev.data.ptr = conn;
            if (conn->closing) {
                std::cerr << "Client disconnected\n";
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->fd, nullptr);
                close(conn->fd);
                delete conn;
                continue;
            }

            new_ev.events = EPOLLIN;
            if (conn->w_len > 0) {
                new_ev.events |= EPOLLOUT;
            }

            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &new_ev);
        }
    }

    close(server_fd);
}

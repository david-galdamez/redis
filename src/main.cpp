#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include "connection.h"
#include <fcntl.h>

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    const int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "Setsockopt failed\n";
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

    ServerConnection redis_server(epoll_fd);

    while (true) {

        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            std::cerr << "Epoll wait failed\n";
            return 1;
        }

        //redis_server.handleTimeouts();

        for (int n = 0; n < nfds; ++n) {

            int current_fd = events[n].data.fd;

            if (current_fd == server_fd) {
                redis_server.acceptClients(server_fd);
                continue;
            }

            if (events[n].events & (EPOLLHUP | EPOLLERR)) {
                redis_server.close(current_fd);
            }

            if (events[n].events & EPOLLIN) {
                bool result = redis_server.handleRead(current_fd);
                if (!result) {
                    redis_server.close(current_fd);
                }
            }

            if (events[n].events & EPOLLOUT) {
                redis_server.handleWrite(current_fd);
            }

            if (redis_server.isClosing(current_fd)) {
                redis_server.handleClose(current_fd);
            }
        }
    }

    close(server_fd);
}
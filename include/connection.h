//
// Created by mamia on 31/12/2025.
//

#ifndef REDIS_CONNECTION_H
#define REDIS_CONNECTION_H
#include <cstddef>
#include <unordered_map>

#endif //REDIS_CONNECTION_H

#pragma once

#define MAX_EVENTS 1024

struct Conn {
    int fd{};

    char r_buffer[4096]{};
    std::size_t r_len = 0;

    char w_buffer[4096]{};
    std::size_t w_len = 0;
    std::size_t w_pos = 0;

    bool closing = false;
};

class ServerConnection {
public:
    ServerConnection(int epoll_fd);
    void acceptClients (int server_fd);
    bool handleRead(int conn_fd);
    bool handleWrite(int conn_fd);
    bool close(int conn_fd);
    bool isClosing(int conn_fd);
    void handleClose(int conn_fd);
private:
    int epoll_fd;
    std::unordered_map<int, Conn *> clients;
};

//
// Created by mamia on 31/12/2025.
//

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "connection.h"
#include <fcntl.h>

ServerConnection::ServerConnection(int epoll_fd) : epoll_fd(epoll_fd) {}


void ServerConnection::acceptClients(int server_fd) {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);

        int conn_fd = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
        if (conn_fd < 0) {
            break;
        }

        int flags = fcntl(conn_fd, F_GETFL, 0);
        fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK);

        std::cout << "Client connected successfully " << conn_fd << std::endl;

        auto new_conn = new Conn();
        new_conn->fd = conn_fd;
        clients[conn_fd] = new_conn;

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = conn_fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
            std::cerr << "Epoll ctl failed\n";
        }
    }
}

bool ServerConnection::handleRead(int conn_fd) {
    if (clients.find(conn_fd) == clients.end()) {
        return false;
    }

    const auto conn = clients[conn_fd];

    ssize_t n = recv(conn->fd, conn->r_buffer + conn->r_len, sizeof(conn->r_buffer) - conn->r_len, 0);
    if (n == 0) {
        conn->closing = true;
        return false;
    }

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }

        conn->closing = true;
        return false;
    }

    conn->r_len += n;

    std::string request(conn->r_buffer, conn->r_len);
    if (request.find("PING") != std::string::npos) {
        const char *response = "+PONG\r\n";
        memcpy(conn->w_buffer, response, strlen(response));
        conn->w_len = strlen(response);
        conn->w_pos = 0;
    }

    if (conn->w_len > 0) {
        epoll_event new_ev{};
        new_ev.data.fd = conn_fd;
        new_ev.events = EPOLLIN | EPOLLOUT;

        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn_fd, &new_ev);
    }

    conn->r_len = 0;
    return true;
}

bool ServerConnection::handleWrite(int conn_fd) {
    if (clients.find(conn_fd) == clients.end()) {
        return false;
    }

    auto conn = clients[conn_fd];

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

    epoll_event new_ev{};
    new_ev.data.fd = conn_fd;
    new_ev.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn_fd, &new_ev);

    return true;
}

bool ServerConnection::close(int conn_fd) {
    if (clients.find(conn_fd) == clients.end()) {
        return false;
    }

    auto conn = clients[conn_fd];
    conn->closing = true;
    return true;
}

bool ServerConnection::isClosing(int conn_fd) {
    return clients[conn_fd]->closing;
}

void ServerConnection::handleClose(int conn_fd) {
    std::cout << "Client disconnected\n";
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn_fd, nullptr);
    close(conn_fd);
    delete clients[conn_fd];
    clients.erase(conn_fd);
}

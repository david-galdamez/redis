//
// Created by mamia on 31/12/2025.
//

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include "parser.h"
#include "connection.h"
#include <fcntl.h>
#include <algorithm>

ServerConnection::ServerConnection(int epoll_fd) : epoll_fd(epoll_fd) {
}

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

        std::cout << "Client connected successfully\n";

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

    Reader reader(request);
    Value parsed_request = reader.parseRequest();
    std::string command = parsed_request.array[0].bulk;

    std::ranges::transform(command, command.begin(), [](const unsigned char c) {
        return std::tolower(c);
    });

    std::string parsed_response;
    std::vector args(parsed_request.array.begin() + 1, parsed_request.array.end());
    Value value_response;

    if (command.find("ping") != std::string::npos) {
        value_response = {.type = DataType::STRING, .string = "PONG"};
    } else if (command.find("echo") != std::string::npos) {
        std::string text = parsed_request.array[1].bulk;
        value_response = {.type = DataType::BULK, .bulk = text};
    } else if (command.find("set") != std::string::npos) {
        if (args.size() < 2) {
            std::cerr << "Invalid arguments\n";
            return false;
        }

        StoragedValue new_value;
        new_value = StoragedValue{
            .value = args[1].bulk,
        };

        if (args.size() > 2) {
            std::vector optional_args(args.begin() + 2, args.end());
            if (optional_args.size() != 2) {
                std::cerr << "Invalid arguments\n";
                return false;
            }

            std::string optional_command = optional_args[0].bulk;
            std::ranges::transform(optional_command, optional_command.begin(), [](const unsigned char c) {
                return std::tolower(c);
            });
            if (optional_command.find("ex") != std::string::npos) {
                int seconds = std::stoi(optional_args[1].bulk);
                if (seconds <= 0) {
                    std::cerr << "Invalid EX value\n";
                    return false;
                }
                auto expires_at = std::chrono::system_clock::now() + std::chrono::seconds(seconds);

                new_value.expirest_at = expires_at;
            } else if (optional_command.find("px") != std::string::npos) {
                int milliseconds = std::stoi(optional_args[1].bulk);
                if (milliseconds <= 0) {
                    std::cerr << "Invalid EX value\n";
                    return false;
                }
                auto expires_at = std::chrono::system_clock::now() + std::chrono::milliseconds(milliseconds);

                new_value.expirest_at = expires_at;
            }
        } else {
            new_value.expirest_at = std::nullopt;
        }
        storage[args[0].bulk] = new_value;

        value_response = {.type = DataType::STRING, .string = "OK"};
    } else if (command.find("get") != std::string::npos) {
        const auto &key = args[0].bulk;

        if (!storage.contains(key)) {
            value_response = {
                .type = DataType::NULLBULK,
            };
        } else {
            auto &value = storage[key];

            if (value.expirest_at) {
                auto now = std::chrono::system_clock::now();
                if (now > *value.expirest_at) {
                    storage.erase(key);
                    value_response = {
                        .type = DataType::NULLBULK,
                    };
                } else {
                    value_response = {
                        .type = DataType::BULK, .bulk = value.value,
                    };
                }
            } else {
                value_response = {
                    .type = DataType::BULK, .bulk = value.value,
                };
            }
        }
    } else if (command.find("rpush") != std::string::npos) {
        if (args.size() < 2) {
            std::cerr << "Invalid arguments\n";
            return false;
        }

        auto& key = args[0].bulk;
        auto& list = lists[key];

        for (int i = 1; i < args.size(); i++) {
            list.push_back(args[i].bulk);
        }

        value_response =  {
            .type = DataType::INTEGER, .integer = static_cast<int>(list.size()),
        };
    } else if (command.find("lrange") != std::string::npos) {
        value_response = {
            .type = DataType::ARRAY,
        };

        if (args.size() < 3) {
            std::cerr << "Invalid arguments\n";
            return false;
        }

        auto& key = args[0].bulk;
        auto& list = lists[key];

        int start = std::stoi(args[1].bulk);
        int end = std::stoi(args[2].bulk);
        int size = static_cast<int>(list.size());

        if (start < 0) start += size;
        if (end < 0) end += size;


        if (start < end || start < size) {

            auto it = list.begin();
            std::advance(it, start);
            for (int i = start; i <= (end >= size - 1 ? size - 1 : end); i++, ++it) {
                Value new_bulk = {
                    .type = DataType::BULK,
                    .bulk = *it,
                };
                value_response.array.push_back(new_bulk);
            }
        }
    } else if (command.find("lpush") != std::string::npos) {
        if (args.size() < 3) {
            std::cerr << "Invalid arguments\n";
            return false;
        }

        auto& key = args[0].bulk;
        auto& list = lists[key];

        for (int i = static_cast<int>(args.size() - 1) ; i > 0 ; i--) {
            list.push_back(args[i].bulk);
        }

        value_response = {
            .type = DataType::INTEGER,
            .integer = static_cast<int>(list.size()),
        };
    } else if (command.find("llen") != std::string::npos) {
        if (args.empty()) {
            std::cerr << "Invalid arguments\n";
            return false;
        }

        value_response = {
            .type = DataType::INTEGER,
        };

        auto& key = args[0].bulk;

        if (!lists.contains(key)) {
            value_response.integer = 0;
        } else {
            auto& list = lists[key];
            value_response.integer = static_cast<int>(list.size());
        }
    } else if (command.find("lpop") != std::string::npos) {
        if (args.empty()) {
            std::cerr << "Invalid arguments\n";
            return false;
        }

        auto& key = args[0].bulk;
        auto& list = lists[key];

        if (list.empty()) {
            value_response = {
                .type = DataType::NULLBULK,
            };
        } else {
            if (args.size() > 1) {
                int num = std::stoi(args[1].bulk) ;
                value_response = {
                    .type = DataType::ARRAY
                };
                for (int i = 0; i < num; i++) {
                    Value new_bulk = {
                        .type = DataType::BULK,
                        .bulk = list.front(),
                    };
                    value_response.array.push_back(new_bulk);
                    list.pop_front();
                }
            } else {
                value_response = {
                    .type = DataType::BULK, .bulk = list.front(),
                };
                list.pop_front();
            }
        }
    }

    parsed_response = value_response.marshal();

    const char *response = parsed_response.c_str();
    memcpy(conn->w_buffer, response, strlen(response));
    conn->w_len = strlen(response);
    conn->w_pos = 0;

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

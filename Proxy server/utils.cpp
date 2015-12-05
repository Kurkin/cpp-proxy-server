//
//  utils.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 23.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "utils.hpp"

client_socket::client_socket(server_socket* server)
{
    struct sockaddr addr;
    socklen_t socklen = sizeof(addr);
    socket = accept(server->get_socket(), &addr, &socklen);
    if (socket == -1) {
        throw_error(errno, "accept()");
    }
    
    const int set = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)) == -1) { // NOSIGPIPE FOR SEND
        throw_error(errno, "setsockopt()");
    };
}

client_socket::client_socket(addrinfo address)
{
    socket = ::socket(address.ai_family, address.ai_socktype, address.ai_protocol);
    if (socket == -1) {
        throw_error(errno, "socket()");
    }
    
    const int set = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)) == -1) { // NOSIGPIPE FOR SEND
        throw_error(errno, "setsockopt()");
    };
    
    int flags;
    if (-1 == (flags = fcntl(socket, F_GETFL, 0)))
        flags = 0;
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw_error(errno, "fcntl()");
    }
    
    std::cout << "trying connect to " << inet_ntoa(*(in_addr*) address.ai_addr) << "\n"; // incorrect
    if (connect(socket,  address.ai_addr, address.ai_addrlen) < 0) {
        if (errno != EINPROGRESS) {
            throw_error(errno, "connect()");
        }
    }
    
    std::cout << "connected " << socket << " \n";
}

server_socket::server_socket(int port): port(port) {
    socket = ::socket(AF_INET, SOCK_STREAM, 0);
}

void server_socket::bind_and_listen()
{
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    int bnd = bind(socket, (sockaddr *)&server, sizeof(server));
    if (bnd == -1) {
        throw_error(errno, "bind()");
    }
    
    if (listen(socket, SOMAXCONN) != 0) {
        throw_error(errno, "listen()");
    }
}
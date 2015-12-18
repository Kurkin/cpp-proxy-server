//
//  socket.cpp
//  proxy
//
//  Created by Kurkin Dmitry on 18.12.15.
//
//

#include <fcntl.h>
#include <arpa/inet.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "socket.hpp"
#include "throw_error.h"

client_socket::client_socket(const server_socket& server)
{
    struct sockaddr addr;
    socklen_t socklen = sizeof(addr);
    fd = accept(server.getfd(), &addr, &socklen);
    if (getfd() == -1) {
        throw_error(errno, "accept()");
    }
    
    const int set = 1;
    if (setsockopt(getfd(), SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)) == -1) { // NOSIGPIPE FOR SEND
        throw_error(errno, "setsockopt()");
    };
    
    int flags;
    if (-1 == (flags = fcntl(getfd(), F_GETFL, 0)))
        flags = 0;
    if (fcntl(getfd(), F_SETFL, flags | O_NONBLOCK) == -1) {
        throw_error(errno, "fcntl()");
    }
}

client_socket::client_socket(const struct addrinfo addrinfo)
{
    fd = ::socket(addrinfo.ai_family, addrinfo.ai_socktype, addrinfo.ai_protocol);
    if (getfd() == -1) {
        throw_error(errno, "socket()");
    }
    
    const int set = 1;
    if (setsockopt(getfd(), SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)) == -1) { // NOSIGPIPE FOR SEND
        throw_error(errno, "setsockopt()");
    };
    
    int flags;
    if (-1 == (flags = fcntl(getfd(), F_GETFL, 0)))
        flags = 0;
    if (fcntl(getfd(), F_SETFL, flags | O_NONBLOCK) == -1) {
        throw_error(errno, "fcntl()");
    }
    
    std::cout << "connecting to " << inet_ntoa(((sockaddr_in *) addrinfo.ai_addr)->sin_addr) << " on sock " << socket << "\n";
    if (connect(getfd(),  addrinfo.ai_addr, addrinfo.ai_addrlen) < 0) {
        if (errno != EINPROGRESS) {
            throw_error(errno, "connect()");
        }
    }
}

server_socket::server_socket(int port): port(port) {
    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (getfd() == -1)
        throw_error(errno, "socket()");
}

void server_socket::bind_and_listen()
{
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    
    int bnd = bind(getfd(), (sockaddr *)&server, sizeof(server));
    if (bnd == -1) {
        throw_error(errno, "bind()");
    }
    
    if (listen(getfd(), SOMAXCONN) != 0) {
        throw_error(errno, "listen()");
    }
}
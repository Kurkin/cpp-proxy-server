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

client_socket::client_socket() noexcept {};

client_socket::client_socket(client_socket&& other) noexcept
{
    std::swap(fd, other.fd);
}

client_socket& client_socket::operator=(client_socket&& rhs) noexcept
{
    std::swap(fd, rhs.fd);
    return *this;
}

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

client_socket::client_socket(const sockaddr addr)
{
    fd = ::socket(AF_INET, SOCK_STREAM, 0);
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
    
    std::cout << "connecting to " << inet_ntoa(((sockaddr_in*) &addr)->sin_addr) << " on sock " << getfd() << "\n";
    if (connect(getfd(),  &addr, sizeof(addr)) < 0) {
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

tcp_client::tcp_client()
    : on_read(on_ready_t{})
    , on_write(on_ready_t{})
    , socket(client_socket{}) {}

tcp_client::tcp_client(client_socket&& socket)
    : on_read(on_ready_t{})
    , on_write(on_ready_t{})
    , socket(std::move(socket)) {}

tcp_client::tcp_client(client_socket&& socket, on_ready_t on_read, on_ready_t on_write)
    : on_read(on_read)
    , on_write(on_write)
    , socket(std::move(socket)) {}

void tcp_client::set_on_read_write(on_ready_t on_read, on_ready_t on_write)
{
    this->on_read = on_read;
    this->on_write = on_write;
}

int tcp_client::get_socket() const noexcept
{
    return socket.getfd();
}

tcp_connection::tcp_connection(io_queue& queue, tcp_client&& client)
    : queue(queue)
    , client(std::move(client))
{
    registrate(this->client);
}

void tcp_connection::set_server(tcp_client &&server)
{
    this->server = std::move(server);
    registrate(server);
}

void tcp_connection::write_to_client(std::string text)
{
    if (client.msg_queue.empty())
        queue.add_event_handler(client.get_socket(), EVFILT_WRITE, client.on_write);
    client.msg_queue.push_back(text);
}

void tcp_connection::write_to_server(std::string text)
{
    if (server.msg_queue.empty() && server.get_socket() != -1)
        queue.add_event_handler(server.get_socket(), EVFILT_WRITE, server.on_write);
    server.msg_queue.push_back(text);
}

int tcp_connection::get_client_socket() const noexcept
{
    return client.get_socket();
}

int tcp_connection::get_server_socket() const noexcept
{
    return server.get_socket();
}

void tcp_connection::set_client_on_read_write(on_ready_t on_read, on_ready_t on_write)
{
    client.set_on_read_write(on_read, on_write);
    update_registration(client);
}

void tcp_connection::set_server_on_read_write(on_ready_t on_read, on_ready_t on_write)
{
    server.set_on_read_write(on_read, on_write);
    update_registration(server);
}

void tcp_connection::registrate(tcp_client &client)
{
    queue.add_event_handler(client.get_socket(), EVFILT_READ, client.on_read);
    if (!client.msg_queue.empty())
        queue.add_event_handler(client.get_socket(), EVFILT_WRITE, client.on_write);
}

void tcp_connection::deregistrate(tcp_client &client)
{
    queue.delete_event_handler(client.get_socket(), EVFILT_READ);
    queue.delete_event_handler(client.get_socket(), EVFILT_WRITE);
}

void tcp_connection::update_registration(tcp_client &client)
{
    deregistrate(client);
    registrate(client);
}

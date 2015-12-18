//
//  socket.hpp
//  proxy
//
//  Created by Kurkin Dmitry on 18.12.15.
//
//

#ifndef socket_hpp
#define socket_hpp

#include "file_descriptor.h"

struct server_socket
{
    server_socket(int port);
    
    int getfd() const noexcept { return fd.getfd(); };
    void bind_and_listen();
    
private:
    file_descriptor fd;
    int port;
};

struct client_socket
{
    client_socket(const struct addrinfo addrinfo); // for connect;
    client_socket(const server_socket& server); // for accept
    
    int getfd() const noexcept { return fd.getfd(); };
    
private:
    file_descriptor fd;
};

#endif /* socket_hpp */

//
//  utils.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 23.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef utils_hpp
#define utils_hpp


#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>
#include <stdio.h>
#include <iostream>

struct server_t {
    server_t(int port);
    ~server_t();
    
    int get_socket();
    void bind_and_listen();
    
private:
    int socket;
    int port;
};

struct client_t {
    
    client_t(in_addr address); // for connect;
    client_t(server_t* server); // for accept
    ~client_t();
    
    int get_socket();
    
private:
    int socket;
};

#endif /* utils_hpp */

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
#include <set>

#define BUFF_SIZE 1024


struct server_t;

struct client_t {
    
    int socket;
    server_t* server = NULL;
    std::string request = "";
    std::string host;
    in_addr addr;
    std::set<int>* open_clients;
    
    client_t(int connecting_socket, std::set<int>* open_clients);
    ~client_t();

    client_t* client_listener_f(struct kevent event);
    client_t* read_request_f(struct kevent event);
    client_t* add_request_text(std::string text);
};

struct server_t {
    
    int socket;
    client_t* client;
    std::set<int>* open_servers;
    
    server_t(client_t* client, std::set<int>* open_servers);
    ~server_t();
    
    void server_listener_f(struct kevent event);
};

struct listen_socket {
    int sock;
    
    listen_socket();
    
    int bind_and_listen(int port);
    
    int get_socket();
};

#endif /* utils_hpp */

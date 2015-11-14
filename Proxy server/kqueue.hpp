//
//  kqueue.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef kqueue_hpp
#define kqueue_hpp

#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>

typedef std::function<void(int)> funct_t;

struct server {
    
    server(int port);
    
    void add_connect_function(funct_t f);
    void add_disconnect_function(funct_t f);
    void add_recv_function(funct_t f);
    
    void watch_loop();
    
private:
    int main_socket;
    int queue;
    funct_t connect_f = [](int d){};
    funct_t disconnect_f = [](int d){};
    funct_t recv_f = [](int d){};
};

void write(int dis, char* message, size_t size);
size_t read(int dis, char* message);
int setNonblocking(int fd);
#endif /* kqueue_hpp */

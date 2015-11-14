//
//  kqueue.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "kqueue.hpp"

server::server(int port) {
    main_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    
    int bnd = bind(main_socket, (sockaddr *)&server, sizeof(server));
    if (bnd == -1) {
        perror("Cant bind port");
        close(main_socket);
        return;
    }
    
    if (listen(main_socket, 128) != 0) {
        close(main_socket);
        perror("Cant lisen");
    }
    
    queue = kqueue();
    
    struct kevent change;
    
    EV_SET(&change, main_socket, EVFILT_READ, EV_ADD, 0, 0, NULL);
    
    if (kevent(queue, &change, 1, NULL, 0, NULL) == -1)
    {
        perror("Cant add socket listener in kqueue");
    }
}
    
void server::add_connect_function(funct_t f) {
    connect_f = f;
}
    
void server::add_disconnect_function(funct_t f) {
    disconnect_f = f;
}
    
void server::add_recv_function(funct_t f) {
    recv_f = f;
}
    
void server::watch_loop() {
    
    struct kevent event;
    struct kevent evList[5];
    struct sockaddr addr;
    socklen_t socklen = sizeof(addr);
    int discriptor;
    
    while (true) {
        printf("in cycle\n");
        int nev = kevent(queue, NULL, 0, evList, 5, NULL);
        if (nev < 1) {
            perror("kqueue error");
            return;
        }
        for (int i = 0; i < nev; i++) {
            if (evList[i].flags & EV_EOF) {
                printf("disconnect\n");
                discriptor = static_cast<int>(evList[i].ident);
                EV_SET(&event, discriptor, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                if (kevent(queue, &event, 1, NULL, 0, NULL) == -1)
                    perror("cant delete event from queue");
                disconnect_f(discriptor);
                close(discriptor);
            } else if (evList[i].ident == main_socket) {
                printf("connect\n");
                discriptor = accept(static_cast<int>(evList[i].ident), &addr, &socklen);
                std::cout << inet_ntoa(((struct sockaddr_in*) &addr)->sin_addr);
                std::cout << "\n";
                if (discriptor == -1)
                    perror("cant get accept");
                EV_SET(&event, discriptor, EVFILT_READ, EV_ADD, 0, 0, NULL);
                if (kevent(queue, &event, 1, NULL, 0, NULL) == -1)
                    perror("cant add event to queue");
                connect_f(discriptor);
            } else {
                printf("message\n");
                recv_f(static_cast<int>(evList[i].ident));
            }
        }
    }
}

void write(int dis, char* message, size_t size) {
    send(dis, message, size, 0);
}

size_t read(int dis, char* message) {
    std::cout << "getting data\n";
//    setNonblocking(dis);
    size_t size = recv(dis, message, 1023, 0);
    std::cout << "ok\n";
    if (size == -1) {
        perror("empty size");
        return 0;
    } else {
        return size;
    }
}

int setNonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}





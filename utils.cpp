//
//  utils.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 23.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "utils.hpp"


client_t::client_t(int connecting_socket, std::set<int>* open_clients): open_clients(open_clients){
    struct sockaddr addr;
    socklen_t socklen = sizeof(addr);
    socket = accept(connecting_socket, &addr, &socklen);
    if (socket == -1) {
        perror("can't get accept client");
        throw "Error in eccept client";
    }
    
    open_clients->insert(socket);
    
    const int set = 1;
    setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));  // NOSIGPIPE FOR SEND
}

client_t::~client_t() {
    close(socket);
    open_clients->erase(socket);
    if (server != NULL) {
        server->client = NULL;
        delete server;
    }
}

client_t* client_t::client_listener_f(struct kevent event) {
    if (event.flags & EV_EOF) {
        std::cout << "client " << socket << " disconnected\n";
        delete this;
        return NULL;
    } else {
        return read_request_f(event);
    }
}

client_t* client_t::read_request_f(struct kevent event) {
    if (server != NULL) {
        server->client = NULL;
        std::cout << "delete old server " << server->socket << "\n";
        delete server;   // !!!!
        server = NULL;
    }
    
    char buff[BUFF_SIZE];
    
    size_t size = read(static_cast<int>(event.ident), buff, BUFF_SIZE);
    if (size == -1) {
        perror("read error");
        return NULL;
    }
    
    return add_request_text(std::string(buff, size));
}

client_t* client_t::add_request_text(std::string text) {

    request.append(text);

    size_t end_of_header;
    
    if ((end_of_header = request.find("\r\n\r\n") == std::string::npos)) {
        return NULL;
    }
    
    std::string body = request.substr(end_of_header + 4);

    std::string method = request.substr(0, request.find(" "));
    
    size_t host_start = request.find("Host: ") + strlen("Host: ");
    size_t host_end = request.find("\r\n", host_start);
    host = request.substr(host_start, host_end - host_start);
    
    std::string content_len;
    
    if (method == "POST") {
        size_t content_len_start = request.find("Content-Length") + strlen("Content-Length");
        size_t content_len_end = request.find("\r\n", content_len_start);
        content_len = request.substr(content_len_start, content_len_end - content_len_start);
        if (body.length() == std::stoi(content_len)) {
            return this;
        }
    } else {
        return this;
    }
    
    return NULL;
}

server_t::server_t(client_t* client, std::set<int>* open_servers): client(client), open_servers(open_servers) {

    socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket == -1) {
        perror("cant open sock");
        throw "can make new sock for server";
    }
    
    open_servers->insert(socket);
    
    const int set = 1;
    setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));  // NOSIGPIPE FOR SEND
    
    sockaddr_in server;
    server.sin_family = AF_INET;
    std::string address = inet_ntoa(client->addr);
    server.sin_addr.s_addr = inet_addr(address.c_str());
    server.sin_port = htons(80);
    std::string name = client->host;
    std::cout << "try to connect to " << name << " " << address << "\n";
    if (connect(socket,  (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("can't connect");
    }
    std::cout << "connected \n";

}

void server_t::server_listener_f(struct kevent event) {
    if (event.flags & EV_EOF && event.data == 0) {
        delete this;
    } else {
        char buff[BUFF_SIZE];
        size_t size = read(socket, buff, BUFF_SIZE);
        send(client->socket, buff, size, 0);
    }
}

server_t::~server_t() {
    if (client != NULL) {
        std::cout << "disconnect server " << socket << " disconnect client " << client->socket << "\n";
        client->server = NULL;
        delete client;
    }
    open_servers->erase(socket);
    close(socket);
}


listen_socket::listen_socket() {
    sock = socket(AF_INET, SOCK_STREAM, 0);
}

int listen_socket::bind_and_listen(int port) {
    
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    int bnd = bind(sock, (sockaddr *)&server, sizeof(server));
    if (bnd == -1) {
        perror("Cant bind port");
        close(sock);
        return -1;
    }
    
    if (listen(sock, 128) != 0) {
        close(sock);
        perror("Cant lisen");
        return -1;
    }
    
    return 0;
}

int listen_socket::get_socket() {
    return sock;
}
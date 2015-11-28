//
//  proxy.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "proxy.hpp"


void proxy_server::resolve(tcp_connection* client) {
    std::lock_guard<std::mutex> lk(queue_mutex);
    host_names.push_back(client);
    queue_ready = true;
    queue_cond.notify_one();
}

proxy_server::proxy_server(io_queue queue, int port): queue(queue) {
    server = new server_t(port);
    server->bind_and_listen();
    
    queue.add_event_handler(server->get_socket(), EVFILT_READ, connect_client_f);
    queue.add_event_handler(USER_EVENT_IDENT, EVFILT_USER, EV_CLEAR, host_resolfed_f);
}

void proxy_server::tcp_connection::client_handler(struct kevent event) {
    if (event.flags & EV_EOF) {
        std::cout << "client " << get_client_sock() << " disconnected\n";
        delete client;
        parent->queue.delete_event_handler(get_client_sock(), EVFILT_READ);
        delete server;
        parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
        delete this;
    } else {
        read_request_f(event);
    }
}

void proxy_server::tcp_connection::read_request_f(struct kevent event) {
    if (server != nullptr) {
        delete server;
        parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
        server = nullptr;
    }

    char buff[BUFF_SIZE];

    size_t size = read(get_client_sock(), buff, BUFF_SIZE);
    if (size == -1) {
        perror("read error");
        return;
    }

    add_request_text(std::string(buff, size));
}

void proxy_server::tcp_connection::add_request_text(std::string text) {

    client->request.append(text);

    size_t end_of_header = client->request.find("\r\n\r\n");

    
    if (end_of_header == std::string::npos) {
        return;
    }
    
    std::string body = client->request.substr(end_of_header + 4);

    std::string method = client->request.substr(0, client->request.find(" "));

    size_t host_start = client->request.find("Host: ") + strlen("Host: ");
    size_t host_end = client->request.find("\r\n", host_start);
    client->host = client->request.substr(host_start, host_end - host_start);

    std::string content_len;

    if (method == "POST") {
        size_t content_len_start = client->request.find("Content-Length: ") + strlen("Content-Length: ");
        size_t content_len_end = client->request.find("\r\n", content_len_start);
        content_len = client->request.substr(content_len_start, content_len_end - content_len_start);
        if (body.length() == std::stoi(content_len)) {
            parent->resolve(this);
        } else {
            std::cout << body << "\n";
            std::cout << body.size() << " body " << std::stoi(content_len) << " content_len\n";
        }
    } else {
        parent->resolve(this);
    }
}

void proxy_server::tcp_connection::make_request() {
    write(get_server_sock(), client->request.c_str(), client->request.length());
    client->request = "";
}

void proxy_server::tcp_connection::server_handler(struct kevent event) {
    if (event.flags & EV_EOF && event.data == 0) {
        delete server;
        parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
        server = nullptr;
    } else {
        char buff[BUFF_SIZE];
        size_t size = read(get_server_sock(), buff, BUFF_SIZE);
        write(get_client_sock(), buff, size);
    }
}

void proxy_server::tcp_connection::connect_to_server() {
    server = new tcp_server(new client_t(client->addr));
}
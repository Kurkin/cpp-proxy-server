//
//  proxy.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "proxy.hpp"
#include <assert.h>

void proxy_server::resolve(parse_state* client) {
//    if (permanent_moved.contain(client->connection->request.URI)) {
//        write(client->connection->get_client_sock(),
//              permanent_moved.get(client->connection->request.URI), permanent_moved.get(client->connection->request.URI).length());
//    }
    std::lock_guard<std::mutex> lk(queue_mutex);
    host_names.push_back(client);
    queue_ready = true;
    queue_cond.notify_one();
}

proxy_server::proxy_server(io_queue queue, int port): queue(queue) {
    server = new server_socket(port);
    server->bind_and_listen();
    
    queue.add_event_handler(server->get_socket(), EVFILT_READ, connect_client_f);
    queue.add_event_handler(USER_EVENT_IDENT, EVFILT_USER, EV_CLEAR, host_resolfed_f);
}

proxy_server::~proxy_server() {
    queue.delete_event_handler(server->get_socket(), EVFILT_READ);
    queue.delete_event_handler(USER_EVENT_IDENT, EVFILT_USER);
    delete server;
}

void proxy_server::tcp_connection::client_handler(struct kevent event) {
    if (event.flags & EV_EOF) {
        std::cout << "EV_EOF from " << event.ident << " client\n";
        std::unique_lock<std::mutex> lk(parent->state);
        if (state) state->canceled = true;
        lk.unlock();
        parent->queue.delete_event_handler(get_client_sock(), EVFILT_READ);
        delete client;
        if (server) parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
        if (server) delete server;
        delete this;
    } else {
        read_request_f(event);
    }
}

void proxy_server::tcp_connection::read_request_f(struct kevent event)
{
    char buff[BUFF_SIZE];

    std::cout << "read request of " << event.ident << "\n";
    size_t size = read(get_client_sock(), buff, BUFF_SIZE);
    if (size == -1) {
        perror("read error");
        return;
    }

    add_request_text(std::string(buff, size));
}

void proxy_server::tcp_connection::add_request_text(std::string text)
{
    client->request.append(text);

    size_t end_of_header = client->request.find("\r\n\r\n");
    
    if (end_of_header == std::string::npos) {
        return;
    }
    
    request req(client->request);

    client->host = req.get_header("Host");
    
    if (req.get_header("Content-Length") != "")
    {
        assert(req.get_body().length() <= std::stoi(req.get_header("Content-Length")));
        if (req.get_body().length() == std::stoi(req.get_header("Content-Length"))) {
            if (state) delete state;
            state = new parse_state(this);
            parent->resolve(state);
        }
    }
    else if (req.get_header("Transfer-Encoding") == "chunked")
        {
            if (std::string(client->request.end() - 5, client->request.end()) == "0\r\n\r\n") {
                if (state) delete state;
                state = new parse_state(this);
                parent->resolve(state);
            }
        }
        else
        {
            if (state) delete state;
            state = new parse_state(this);
            parent->resolve(state);
        }
}

void proxy_server::tcp_connection::connect_to_server()
{
    if (server) {
        if (inet_ntoa(client->addr) == inet_ntoa(server->addr)) {
            std::cout << "keep-alive is working!\n";
            return;
        } else {
            std::cout << "delete old server" << get_server_sock() << "\n";
            parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
            delete server;
            server = nullptr;
        }
    }
    server = new tcp_server(new client_socket(client->addr));
}

void proxy_server::tcp_connection::make_request()
{
    parent->queue.add_event_handler(get_server_sock(), EVFILT_WRITE, [this](struct kevent event){
        write(get_server_sock(), client->request.c_str(), client->request.length());
        client->request = "";
        parent->queue.delete_event_handler(get_server_sock(), EVFILT_WRITE);
    });
}

void proxy_server::tcp_connection::server_handler(struct kevent event)
{
    if (event.flags & EV_EOF && event.data == 0) {
        std::cout << "EV_EOF from " << event.ident << " server\n";
        parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
        delete server;
        server = nullptr;
    } else {
        char buff[event.data];
        std::cout << "transfer from " << get_server_sock() << " to " << get_client_sock() << "\n";
        size_t size = read(get_server_sock(), buff, event.data);
        write(get_client_sock(), buff, size);
    }
}
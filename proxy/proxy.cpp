//
//  proxy.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include <assert.h>

#include "proxy.hpp"
#include "throw_error.h"
#include "new_http_handler.hpp"

void proxy_server::resolve(parse_state* state)
{
    std::lock_guard<std::mutex> lk(queue_mutex);
    host_names.push_back(state);
    queue_ready = true;
    queue_cond.notify_one();
}

proxy_server::proxy_server(io_queue& queue, int port): queue(queue), cache(10000)
{
    server = new server_socket(port);
    server->bind_and_listen();
    
    queue.add_event_handler(server->get_socket(), EVFILT_READ, connect_client_f);
    queue.add_event_handler(USER_EVENT_IDENT, EVFILT_USER, EV_CLEAR, host_resolfed_f);
}

proxy_server::~proxy_server()
{
    queue.delete_event_handler(server->get_socket(), EVFILT_READ);
    queue.delete_event_handler(USER_EVENT_IDENT, EVFILT_USER);
    delete server;
}

void proxy_server::tcp_connection::client_handler(struct kevent event)
{
    if (event.flags & EV_EOF) {
        std::cout << "EV_EOF from " << event.ident << " client\n";
        std::unique_lock<std::mutex> lk(parent->state);
        if (state) state->canceled = true;
        lk.unlock();
        parent->queue.delete_event_handler(get_client_sock(), EVFILT_READ);
        delete client;
        if (server) {
            parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
            delete server;
        }
        delete this;
    } else {
        read_request(event);
    }
}

void proxy_server::tcp_connection::read_request(struct kevent event)
{
    char buff[event.data];
    
    std::cout << "read request of " << event.ident << "\n";
    size_t size = read(get_client_sock(), buff, event.data);
    if (size == static_cast<size_t>(-1)) {
        throw_error(errno, "read()");
    }
    std::cout << std::string(buff, size) << "\n";
    if (client->request == nullptr) {
        client->request = new request({buff, size});
    } else {
        client->request->add_part({buff, size});
    }
    std::cout << "added to requests\n";
    if (client->request->get_state() == BAD) {
        std::cout << "Bad Request\n";
        write(get_client_sock(), "HTTP/1.1 400 Bad Request\r\n\r\n");
        parent->queue.delete_event_handler(get_client_sock(), EVFILT_READ);
    }
    if (client->request->get_state() == FULL_BODY) {
        std::cout << "push to resolve\n";
        if (get_host() == "") {
            throw std::runtime_error("empty host 1");
        }
        if (state) delete state;
        state = new parse_state(this);
        parent->resolve(state);
    }
}

void proxy_server::tcp_connection::connect_to_server()
{
    if (server) {
        if (client->request->get_host() == server->host)
        {
            std::cout << "keep-alive is working!\n";
            try_to_cache();
            delete server->response;
            server->response = nullptr;
            server->URI = client->request->get_URI();
            return;
        } else {
            std::cout << "delete old server" << get_server_sock() << "\n";
            parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
            try_to_cache();
            delete server;
            server = nullptr;
        }
    }
    
    server = new tcp_server(new client_socket(client->addrinfo));
    server->host = client->request->get_host();
    server->URI = client->request->get_URI();
}

void proxy_server::tcp_connection::make_request()
{
    if (!client->request->is_validating() && parent->cache.contain(get_host() + get_URI())) {
        std::cout << "cache is working!\n"; // TODO: validate cache use if_match
        auto cache_response =  parent->cache.get(get_host() + get_URI());
        write(get_client_sock(), cache_response);
        return;
    }
    
    std::cout << "tcp_pair: client: " << get_client_sock() << " server: " << get_server_sock() << "\n";
    
    write(get_server_sock(), client->request->get_request_text());
    delete client->request;
    client->request = nullptr;
    parent->queue.add_event_handler(get_server_sock(), EVFILT_READ, [this](struct kevent event){
        server_handler(event);
    });
}

void proxy_server::tcp_connection::server_handler(struct kevent event)
{
    if (event.flags & EV_EOF && event.data == 0) {
        std::cout << "EV_EOF from " << event.ident << " server\n";
        try_to_cache();
        parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
        msg_to_server.erase(msg_to_server.begin(), msg_to_server.end());
        delete server;
        server = nullptr;
    } else {
        char buff[event.data];
        std::cout << "read from " << get_server_sock() << "\n";
        size_t size = recv(get_server_sock(), buff, event.data, 0);
        if (server->response == nullptr) {
            server->response = new response({buff,size});
        } else {
            server->response->add_part({buff, size});
        }
        write(get_client_sock(), {buff, size});
    }
}

void proxy_server::tcp_connection::try_to_cache()
{
    if (server->response->is_cacheable()) {
        std::cout << "add to cache: " << server->host + server->URI  <<  " " << server->response->get_header("ETag") << "\n";
        parent->cache.put(server->host + server->URI, server->response->get_text());
    }
}

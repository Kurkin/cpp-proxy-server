//
//  proxy.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "proxy.hpp"
#include <assert.h>

void proxy_server::resolve(parse_state* state) {
    if (permanent_moved.contain(state->connection->get_host() + state->connection->get_URI())) {
        auto ans = permanent_moved.get(state->connection->get_host() + state->connection->get_URI());
        std::cout << "permanent_moved working\n" << ans;
        write(state->connection->get_client_sock(), ans.c_str(), ans.size());
        return;
    }
    std::lock_guard<std::mutex> lk(queue_mutex);
    host_names.push_back(state);
    queue_ready = true;
    queue_cond.notify_one();
}

proxy_server::proxy_server(io_queue queue, int port): queue(queue), permanent_moved(10000), cache(10000) {
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
        if (server) {
            parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
            delete server;
        }
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
    client->URI = req.get_URI();
    
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
            if (std::string(client->request.end() - 7, client->request.end()) == "\r\n0\r\n\r\n") {
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
        if (client->host == server->host)
        {
            std::cout << "keep-alive is working!\n";
            try_to_cache(server);
            server->response = "";
            server->URI = client->URI;
            return;
        } else {
            std::cout << "delete old server" << get_server_sock() << "\n";
            parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
            try_to_cache(server);
            delete server;
            server = nullptr;
        }
    }
    server = new tcp_server(new client_socket(client->addrinfo));
    server->host = client->host;
    server->URI = client->URI;
}

void proxy_server::tcp_connection::make_request()
{
    
//    if (parent->cache.contain(get_host() + get_URI())) {
//        struct response response = parent->cache.get(get_host() + get_URI());
//        auto cache_response = response.make_cache_response();
//        std::cout << "ans from cache to: " << get_host() + get_URI() << "\n";
//        std::cout << client->request;
//        write(get_client_sock(), cache_response.c_str(), cache_response.size());
//        return;
//    }
    
    parent->queue.add_event_handler(get_server_sock(), EVFILT_WRITE, [this](struct kevent event){
        write(get_server_sock(), client->request.c_str(), client->request.length());
        server->request = client->request;
        client->request = "";
        parent->queue.delete_event_handler(get_server_sock(), EVFILT_WRITE);
        
        parent->queue.add_event_handler(get_server_sock(), EVFILT_READ, [this](struct kevent event){
            server_handler(event);
        });
    });
}

void proxy_server::tcp_connection::server_handler(struct kevent event)
{
    if (event.flags & EV_EOF && event.data == 0) {
        std::cout << "EV_EOF from " << event.ident << " server\n";
        try_to_cache(server);
        parent->queue.delete_event_handler(get_server_sock(), EVFILT_READ);
        delete server;
        server = nullptr;
    } else {
        char buff[event.data];
        std::cout << "transfer from " << get_server_sock() << " to " << get_client_sock() << "\n";
        size_t size = read(get_server_sock(), buff, event.data);
        server->response.append({buff, size});
        write(get_client_sock(), buff, size);
    }
}

void proxy_server::tcp_connection::try_to_cache(proxy_server::tcp_connection::tcp_server *server)
{
    size_t end_of_header = server->response.find("\r\n\r\n");
    
    if (end_of_header == std::string::npos) {
        return;
    }
    
    auto response = std::make_shared<struct response>(server->response);
    
    if (std::stoi(response->get_code()) == 301) {
        parent->permanent_moved.put(get_host() + get_URI(), response->make_redirect_response());
    }
    
    if (response->get_header("ETag") != "" && ((response->get_header("Content-Length") != ""
        && response->get_body().length() == std::stoi(response->get_header("Content-Length")))
        || (response->get_header("Transfer-Encoding") == "chunked"
        && std::string(server->response.end() - 7, server->response.end()) == "\r\n0\r\n\r\n"))
        && response->get_header("Vary") == "" && response->get_code() == "200")
    {
        std::cout << "add to cachce: " << server->host + server->URI << "\n";
        parent->cache.put(server->host + server->URI, *response);
    }
}
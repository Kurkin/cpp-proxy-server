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

namespace
{
    void write_some(tcp_client& dest, io_queue& queue, int ident)
    {
        if (dest.msg_queue.empty()) {
            queue.delete_event_handler(ident, EVFILT_WRITE);
            return;
        }
        write_part part = dest.msg_queue.front();
        dest.msg_queue.pop_front();
        std::cout << "write to " << ident << "\n";
        size_t writted = ::write(ident, part.get_part_text(), part.get_part_size());
        if (writted == -1) {
            if (errno != EPIPE) {
                throw_error(errno, "write()");
            } else {
                if (part.get_part_size() != 0) {
                    dest.msg_queue.push_front(part);
                }
            }
        } else {
            part.writted += writted;
            if (part.get_part_size() != 0) {
                dest.msg_queue.push_front(part);
            }
        }
    }
}

void proxy_server::resolve(parse_state* state)
{
    std::lock_guard<std::mutex> lk(queue_mutex);
    host_names.push_back(state);
    queue_ready = true;
    queue_cond.notify_one();
}

proxy_server::proxy_server(io_queue& queue, int port): server(server_socket(port)), queue(queue), cache(10000)
{
    server.bind_and_listen();
    
    queue.add_event_handler(server.getfd(), EVFILT_READ, connect_client);
    queue.add_event_handler(USER_EVENT_IDENT, EVFILT_USER, EV_CLEAR, host_resolfed);
}

proxy_server::~proxy_server()
{
    queue.delete_event_handler(server.getfd(), EVFILT_READ);
    queue.delete_event_handler(USER_EVENT_IDENT, EVFILT_USER);
}

proxy_server::proxy_tcp_connection::proxy_tcp_connection(proxy_server& proxy, io_queue& queue, tcp_client&& client)
    : tcp_connection(queue, std::move(client))
    , timer(this->queue.get_timer(), timeout, [this]() {
        std::cout << "timeout for " << get_client_socket() << "\n";
        delete this;
    })
    , proxy(proxy)
{}

proxy_server::proxy_tcp_connection::~proxy_tcp_connection()
{
    if (request)
        delete request;
    if (response)
        delete response;
    if (state)
        delete state;
}

std::string proxy_server::proxy_tcp_connection::get_host() const noexcept
{
    return request->get_host();
}

void proxy_server::proxy_tcp_connection::set_client_addr(sockaddr addr)
{
    client_addr = addr;
}

void proxy_server::proxy_tcp_connection::connect_to_server()
{
    if (get_server_socket() != -1) {
        if (request->get_host() == host)
        {
            std::cout << "keep-alive is working!\n";
            try_to_cache();
            delete response;
            response = nullptr;
            URI = request->get_URI();
            return;
        } else {
            deregistrate(server);
        }
    }
    
    server = tcp_client(client_socket(client_addr));
    host = request->get_host();
    URI = request->get_URI();
 
    set_server_on_read_write(
        [this](struct kevent event)
        { server_on_read(event); },
        [this](struct kevent event)
        { server_on_write(event); });
}

void proxy_server::proxy_tcp_connection::client_on_read(struct kevent event)
{
    if (event.flags & EV_EOF)
    {
        std::cout << "EV_EOF from " << event.ident << " client\n";
        delete this;
    } else
    {
        timer.restart(queue.get_timer(), timeout);
        char buff[event.data];
        
        std::cout << "read request of " << event.ident << "\n";
        
        size_t size = read(get_client_socket() , buff, event.data);
        if (size == static_cast<size_t>(-1)) {
            throw_error(errno, "read()");
        }
        if (request) {
            request->add_part({buff, size});
        } else {
            request = new struct request({buff, size});
        }
        
        if (request->get_state() == BAD)
        {
//                    todo: non block write
//                        std::cout << "Bad Request\n";
//                        write(get_client_sock(), "HTTP/1.1 400 Bad Request\r\n\r\n");
            delete this;
            return;
        }
        if (request->get_state() == FULL_BODY)
        {
            std::cout << "push to resolve " << get_host() << request->get_URI() << "\n";
            if (state) delete state;
            state = new parse_state(this);
            proxy.resolve(state);
        }
    }
}

void proxy_server::proxy_tcp_connection::client_on_write(struct kevent event)
{
    timer.restart(queue.get_timer(), timeout);
    write_some(client, queue, event.ident);
}

void proxy_server::proxy_tcp_connection::server_on_write(struct kevent event)
{
    timer.restart(queue.get_timer(), timeout);
    write_some(server, queue, event.ident);
}

void proxy_server::proxy_tcp_connection::server_on_read(struct kevent event)
{
    if (event.flags & EV_EOF && event.data == 0) {
        std::cout << "EV_EOF from " << event.ident << " server\n";
        try_to_cache();
        deregistrate(server);
        server = client_socket();
    } else {
        timer.restart(queue.get_timer(), timeout);
        char buff[event.data];
        std::cout << "read from " << get_server_socket() << "\n";
        size_t size = recv(get_server_socket(), buff, event.data, 0);
        if (response == nullptr) {
            response = new struct response({buff,size});
        } else {
            response->add_part({buff, size});
        }
        write_to_client({buff, size});
    }
}

void proxy_server::proxy_tcp_connection::make_request()
{
    if (!request->is_validating() && proxy.cache.contain(request->get_host() + request->get_URI())) {
        std::cout << "cache is working! for " << get_client_socket() << "\n"; // TODO: validate cache use if_match
        auto cache_response =  proxy.cache.get(request->get_host() + request->get_URI());
        write_to_client(cache_response);
        delete request;
        request = nullptr;
        return;
    }
    
    std::cout << "tcp_pair: client: " << get_client_socket() << " server: " << get_server_socket() << "\n";
    
    write_to_server(request->get_request_text());
    delete request;
    request = nullptr;
}

void proxy_server::proxy_tcp_connection::try_to_cache()
{
    if (response != nullptr && response->is_cacheable()) {
        std::cout << "add to cache: " << host + URI  <<  " " << response->get_header("ETag") << "\n";
        proxy.cache.put(host + URI, response->get_text());
    }
}

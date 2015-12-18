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

proxy_server::proxy_tcp_connection::proxy_tcp_connection(io_queue& queue, tcp_client&& client)
    : tcp_connection(queue, std::move(client))
    , timer(this->queue.get_timer(), timeout, [this]() {
        std::cout << "timeout for " << get_client_socket() << "\n";
        delete this;
    })
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

void proxy_server::proxy_tcp_connection::set_client_addrinfo(addrinfo addr)
{
    client_addr = addr;
}

//void proxy_server::tcp_connection::make_request()
//{
//    if (!client->request->is_validating() && parent->cache.contain(get_host() + get_URI())) {
//        std::cout << "cache is working! for " << get_client_sock() << "\n"; // TODO: validate cache use if_match
//        auto cache_response =  parent->cache.get(get_host() + get_URI());
//        write(get_client_sock(), cache_response);
//        delete client->request;
//        client->request = nullptr;
//        return;
//    }
//    
//    std::cout << "tcp_pair: client: " << get_client_sock() << " server: " << get_server_sock() << "\n";
//    
//    write(get_server_sock(), client->request->get_request_text());
//    delete client->request;
//    client->request = nullptr;
//    parent->queue.add_event_handler(get_server_sock(), EVFILT_READ, [this](struct kevent event){
//        server_handler(event);
//    });
//}


//void proxy_server::tcp_connection::try_to_cache()
//{
//    if (server->response != nullptr && server->response->is_cacheable()) {
//        std::cout << "add to cache: " << server->host + server->URI  <<  " " << server->response->get_header("ETag") << "\n";
//        parent->cache.put(server->host + server->URI, server->response->get_text());
//    }
//}

//
//  proxy.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef proxy_hpp
#define proxy_hpp

#include <memory>
#include <list>
#include <mutex>
#include <condition_variable>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <deque>
#include <arpa/inet.h>
#include <thread>

#include "kqueue.hpp"
#include "utils.hpp"
#include "new_http_handler.hpp"
#include "throw_error.h"
#include "DNSresolver.hpp"
#include "socket.hpp"

uintptr_t const ident = 0x5c0276ef;

struct proxy_server
{
private:
    struct proxy_tcp_connection;
    struct parse_state;

    std::map<proxy_tcp_connection*, std::unique_ptr<proxy_tcp_connection>> connections;

    // TODO: Almost all of data members below have nothing to do with proxy server
    //       and can be grouped together into a class called "resolver".
    //       An important point is that this class should know nothing about
    //       proxy_tcp_connection. This will allow the resolver class to be used
    //       anywhere and not only in the proxy_server.
    //
    //       Also this change will simplify the proxy_server class significantly.
    
    std::mutex resolved_mutex;
    server_socket server;
    io_queue& queue;
    DNSresolver& resolver;
    
    lru_cache<std::string, response> cache;
    
public:
    proxy_server(io_queue& queue, int port, DNSresolver& resolver);
    ~proxy_server();

private:
    struct proxy_tcp_connection : tcp_connection
    {
        proxy_tcp_connection(proxy_server& proxy, io_queue& queue, tcp_client client);
        ~proxy_tcp_connection();
        
        std::string get_host() const noexcept;
        void set_client_addr(const sockaddr& addr);
        void set_client_addr(const sockaddr&& addr);
        void connect_to_server();
        void client_on_write(struct kevent event);
        void client_on_read(struct kevent event);
        void server_on_write(struct kevent event);
        void server_on_read(struct kevent event);
        void CONNECT_on_read(struct kevent event);
        void on_resolver_hostname(struct kevent event);
        void make_request();
        void try_to_cache();
        
        std::unique_ptr<response> response;
        std::unique_ptr<request> request;
        resolve_state state;
        std::string host;
        std::string URI;
        sockaddr client_addr;
        timer_element timer;
        proxy_server& proxy;
    };
};

#endif /* proxy_hpp */
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
#include "socket.hpp"

uintptr_t const ident = 0x5c0276ef;

struct proxy_server
{
private:
    struct proxy_tcp_connection;
    struct parse_state;
    
    std::map<proxy_tcp_connection*, std::unique_ptr<proxy_tcp_connection>> connections;

    std::list<std::unique_ptr<parse_state>> host_names;
    std::list<std::unique_ptr<parse_state>> ans;
    std::condition_variable queue_cond;
    std::mutex queue_mutex;
    std::mutex ans_mutex;
    std::mutex state;
    std::mutex cache_mutex;
    std::vector<std::thread> resolvers;
    
    server_socket server;
    io_queue& queue;
    
    lru_cache<std::string, response> cache; // todo: <uri, cache ans>, cache ans should be a struct
    lru_cache<std::string, sockaddr> addr_cache;
    
public:
    proxy_server(io_queue& queue, int port, size_t resolvers_num);
    ~proxy_server();

    void resolve(std::unique_ptr<parse_state> connection);

private:
    void resolver_thread_proc();

    struct parse_state
    {
        parse_state(proxy_tcp_connection* connection) : connection(connection) {};
        proxy_tcp_connection* connection;
        bool canceled = false;
    };
    
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
        void make_request();
        void try_to_cache();
        
        std::unique_ptr<response> response;
        std::unique_ptr<request> request;
        parse_state* state = nullptr;
        std::string host;
        std::string URI;
        sockaddr client_addr;
        timer_element timer;
        proxy_server& proxy;
        bool revalidate = false;
    };
};

#endif /* proxy_hpp */
//
//  proxy.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef proxy_hpp
#define proxy_hpp

#include <list>
#include <mutex>
#include <condition_variable>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <deque>
#include <arpa/inet.h>

#include "kqueue.hpp"
#include "utils.hpp"
#include "new_http_handler.hpp"
#include "throw_error.h"
#include "socket.hpp"

#define BUFF_SIZE 1024
#define USER_EVENT_IDENT 0x5c0276ef

namespace
{
    constexpr const timer::clock_t::duration timeout = std::chrono::seconds(5);
}

struct proxy_server
{

private:
    struct proxy_tcp_connection;
    struct parse_state;
    std::list<parse_state*> host_names;
    std::mutex queue_mutex;
    std::condition_variable queue_cond;
    bool queue_ready;

    std::mutex ans_mutex;
    std::list<parse_state*> ans;

    std::mutex state;

    server_socket server;

    io_queue& queue;

    lru_cache<std::string, std::string> cache; // todo: <uri, cache ans>, cache ans should be a struct

public:
    proxy_server(io_queue& queue, int port);
    ~proxy_server();

    void resolve(parse_state* connection);

    std::function<void()> resolver = [&](){

        lru_cache<std::string, sockaddr> cache(1000);
        while (true) {
            std::unique_lock<std::mutex> lk(queue_mutex);
            queue_cond.wait(lk, [&]{return queue_ready;});

            parse_state* parse_ed = host_names.front();
            host_names.pop_front();
            if (host_names.size() == 0) {
                queue_ready = false;
            }
            lk.unlock();

            std::string name;
            {
                std::unique_lock<std::mutex> lk1(state);
                if (!parse_ed->canceled) {
                    name = parse_ed->connection->get_host();
                } else {
                    delete parse_ed;
                    continue;
                }
            }

            std::string port = "80";
            if (name.find(":") != static_cast<size_t>(-1)) {
                size_t port_str = name.find(":");
                port = name.substr(port_str + 1);
                name = name.erase(port_str);
            }
            
            if (cache.contain(name + port)) {
                std::unique_lock<std::mutex> lk1(ans_mutex);
                std::unique_lock<std::mutex> lk2(state);
                if (!parse_ed->canceled) {
                    parse_ed->connection->set_client_addr(cache.get(name + port));
                } else {
                    delete parse_ed;
                    continue;
                }
                ans.push_back(parse_ed);
                queue.trigger_user_event_handler(USER_EVENT_IDENT);
                lk2.unlock();
                lk1.unlock();
                continue;
            }

            struct addrinfo hints, *res;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = hints.ai_flags | AI_NUMERICSERV;
            
            int error = getaddrinfo(name.c_str(), port.c_str(), &hints, &res);
            if (error) {
                std::cout << name + ":" + port << "\n";
                perror(gai_strerror(error));
                continue;
            }
            
            sockaddr resolved = *(res->ai_addr);
            freeaddrinfo(res);
            
            std::unique_lock<std::mutex> lk1(ans_mutex);
            std::unique_lock<std::mutex> lk2(state);
                if (!parse_ed->canceled) {
                    parse_ed->connection->set_client_addr(resolved);
                    cache.put(name + port, resolved);
                } else {
                    delete parse_ed;
                    continue;
                }
                ans.push_back(parse_ed);
                queue.trigger_user_event_handler(USER_EVENT_IDENT);
            lk2.unlock();
            lk1.unlock();

            queue_cond.notify_one();
        }
    };

    funct_t host_resolfed = [&](struct kevent event)
    {
        parse_state* parse_ed;
        {
            std::unique_lock<std::mutex> lk(ans_mutex);
            if (ans.size() == 0) {
                return;
            }
            parse_ed = ans.front();
            ans.pop_front();
        }

        {
            std::unique_lock<std::mutex> lk(state);
            if (parse_ed->canceled) {
                delete parse_ed;
                return;
            }
        }

        proxy_tcp_connection* connection = parse_ed -> connection;
        connection -> state = nullptr;
        delete parse_ed;

        std::cout << "host resolved \n";
        connection->connect_to_server();
        connection->make_request();
        
        std::unique_lock<std::mutex> lk(ans_mutex);
        if (ans.size() != 0) {
            queue.trigger_user_event_handler(USER_EVENT_IDENT);
        }
    };
    
    funct_t connect_client = [this](struct kevent event) {
        proxy_tcp_connection* connection = new proxy_tcp_connection(*this, queue, tcp_client(client_socket(server)));
        connection->set_client_on_read_write(
            [connection](struct kevent event)
            { connection->client_on_read(event); },
            [connection](struct kevent event)
            { connection->client_on_write(event); });
    };
    
private:
    struct parse_state
    {
        parse_state(proxy_tcp_connection* connection) : connection(connection) {};
        proxy_tcp_connection* connection;
        bool canceled = false;
    };
    
    struct proxy_tcp_connection : tcp_connection
    {
        proxy_tcp_connection(proxy_server& proxy, io_queue& queue, tcp_client&& client);
        ~proxy_tcp_connection();
        
        std::string get_host() const noexcept;
        void set_client_addr(sockaddr addr);
        void connect_to_server();
        void client_on_write(struct kevent event);
        void client_on_read(struct kevent event);
        void server_on_write(struct kevent event);
        void server_on_read(struct kevent event);
        void make_request();
        void try_to_cache();
        
        struct request* request = nullptr;
        struct response* response = nullptr;
        parse_state* state = nullptr;
        std::string host;
        std::string URI;
        sockaddr client_addr;
        timer_element timer;
        proxy_server& proxy;
    };
};

#endif /* proxy_hpp */
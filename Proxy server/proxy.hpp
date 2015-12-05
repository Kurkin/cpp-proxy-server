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
#include <netdb.h>
#include <err.h>

#include "kqueue.hpp"
#include "utils.hpp"
#include "http_handler.hpp"

#define BUFF_SIZE 1024
#define USER_EVENT_IDENT 0x5c0276ef

struct proxy_server {
    
private:
    struct tcp_connection;
    struct parse_state;
    std::list<parse_state*> host_names;
    std::mutex queue_mutex;
    std::condition_variable queue_cond;
    bool queue_ready;
    
    std::mutex ans_mutex;
    std::list<parse_state*> ans;
    
    std::mutex state;
    
    server_socket* server;
    
    io_queue& queue;
    
    lru_cache<std::string, response> cache;
    lru_cache<std::string, std::string> permanent_moved;
    
public:
    proxy_server(io_queue& queue, int port);
    ~proxy_server();
    
    void resolve(parse_state* connection);
    
    std::function<void()> resolver = [&](){
        
        lru_cache<std::string, addrinfo> cache(1000);
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

            if (cache.contain(name)) {
                std::unique_lock<std::mutex> lk1(ans_mutex);
                std::unique_lock<std::mutex> lk2(state);
                if (!parse_ed->canceled) {
                    parse_ed->connection->set_addrinfo(cache.get(name));
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
            hints.ai_family = PF_INET;
            hints.ai_socktype = SOCK_STREAM;
            int error = getaddrinfo(name.c_str(), "http", &hints, &res);
            if (error) {
                errx(1, "%s", gai_strerror(error));
            }
            
            std::unique_lock<std::mutex> lk1(ans_mutex);
            std::unique_lock<std::mutex> lk2(state);
                if (!parse_ed->canceled) {
                    parse_ed->connection->set_addrinfo(*res);
                    cache.put(name, *res);
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
    
    parse_state* get_next()
    {
        std::unique_lock<std::mutex> lk1(ans_mutex);
        parse_state* state = ans.front();
        ans.pop_front();
        return state;
    }
    
    funct_t host_resolfed_f = [&](struct kevent event)
    {
        parse_state* parse_ed = get_next();
        
        {
            std::unique_lock<std::mutex> lk1(state);
            if (parse_ed->canceled) {
                delete parse_ed;
                return;
            }
        }
        
        tcp_connection* connection = parse_ed -> connection;
        connection -> state = nullptr;
        delete parse_ed;
        
        connection -> connect_to_server();
        connection -> make_request();
        
        std::unique_lock<std::mutex> lk2(ans_mutex);
        if (ans.size() != 0) {
            queue.trigger_user_event_handler(USER_EVENT_IDENT);
        }
    };
    
    funct_t connect_client_f = [&](struct kevent event) {
        tcp_connection* connection = new tcp_connection(new client_socket(server), this);
        std::cout << "client connected: " << connection->get_client_sock() << "\n";
        queue.add_event_handler(connection->get_client_sock(), EVFILT_READ, [connection](struct kevent event){
            connection->client_handler(event);
        });
    };
    
private:
    
    struct parse_state
    {
        parse_state(tcp_connection* connection) : connection(connection) {};
        tcp_connection* connection;
        bool canceled = false;
    };
    
    struct tcp_connection
    {
    private:
        struct tcp_client;
        struct tcp_server;
        tcp_client* client = nullptr;
        tcp_server* server = nullptr;
        proxy_server* parent;
        
    public:
        tcp_connection(client_socket* client, proxy_server* parent) : client(new tcp_client(client)), parent(parent) {};
        ~tcp_connection() { std::cout << "tcp_connect deleted\n"; }
        int get_client_sock() { return client->get_socket(); }
        int get_server_sock() { return server->get_socket(); }
        std::string get_host() { return client->host; }
        std::string get_URI() { return client->URI; }
        void set_addrinfo(struct addrinfo addrinfo) { client->addrinfo = addrinfo; }
        void connect_to_server();
        void make_request();
        void try_to_cache(tcp_server* server);
        void server_handler(struct kevent event);
        void client_handler(struct kevent event);
        
        parse_state* state = nullptr;
        
    private:
        void add_request_text(std::string text);
        void read_request_f(struct kevent event);

        struct tcp_client {
            tcp_client(client_socket* client) : socket(client) {};
            ~tcp_client()
            {
                std::cout << socket->get_socket() << " client deleted\n";
                delete socket;
            };
            
            client_socket* socket;
            
            int get_socket() { return socket->get_socket(); }
            
            std::string request;
            std::string host;
            std::string URI;
            struct addrinfo addrinfo;
        };
        
        struct tcp_server
        {
            tcp_server(client_socket* socket) : socket(socket) {}
            ~tcp_server()
            {
                std::cout << socket->get_socket() << " server deleted\n";
                delete socket;
            }
            
            int get_socket() { return socket->get_socket(); }
            
            client_socket* socket;
            struct addrinfo addrinfo;
            std::string response;
            std::string request;
            std::string host;
            std::string URI;
        };
    };
};




#endif /* proxy_hpp */

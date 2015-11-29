//
//  proxy.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef proxy_hpp
#define proxy_hpp

#include <stdio.h>
#include <list>
#include <mutex>
#include <condition_variable>
#include "kqueue.hpp"
#include "utils.hpp"
#include <set>
#include <thread>
#include <chrono>

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
    
    io_queue queue;
    
public:
    proxy_server(io_queue queue, int port);
    ~proxy_server();
    
    void resolve(parse_state* connection);
    
    std::function<void()> resolver = [&](){
        
        lru_cache<std::string, in_addr> cache(1000);
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

            if (name.find(":") != std::string::npos) {
                name.erase(name.find(":"));
            }
            
            if (cache.contain(name)) {
                std::unique_lock<std::mutex> lk1(ans_mutex);
                std::unique_lock<std::mutex> lk2(state);
                if (!parse_ed->canceled) {
                    parse_ed->connection->set_addr(cache.get(name));
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
            
            struct hostent* addr;
            
            if ((addr = gethostbyname(name.c_str())) == NULL) {
                perror("resolve fail");
                std::cout << name << "\n";
                continue; //resolve error, just skip this host
            }
            
            
            std::unique_lock<std::mutex> lk1(ans_mutex);
            std::unique_lock<std::mutex> lk2(state);
                if (!parse_ed->canceled) {
                    parse_ed->connection->set_addr(*(in_addr*)addr->h_addr_list[0]);
                    cache.put(name, *(in_addr*)addr->h_addr_list[0]);
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
    
    parse_state* get_next() {
        std::unique_lock<std::mutex> lk1(ans_mutex);
        parse_state* connection = ans.front();
        ans.pop_front();
        return connection;
    }
    
    funct_t host_resolfed_f = [&](struct kevent event){

        parse_state* parse_ed = get_next();
        
        {
            std::unique_lock<std::mutex> lk1(state);
            if (parse_ed->canceled) {
                delete parse_ed;
                return;
            }
        }
        
        parse_ed -> connection -> connect_to_server();
        parse_ed -> connection -> make_request();
        
        tcp_connection* temp = parse_ed -> connection;
        temp -> state = nullptr;
        delete parse_ed;
        
        queue.add_event_handler(temp -> get_server_sock(), EVFILT_READ, [temp](struct kevent event){
            temp -> server_handler(event);
        });
        
        queue.add_event_handler(temp -> get_client_sock(), EVFILT_READ, [temp](struct kevent event){
            temp -> client_handler(event);
        });
        
        
        
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
    
    struct parse_state {
        parse_state(tcp_connection* connection) : connection(connection) {};
        tcp_connection* connection;
        bool canceled = false;
    };
    
    struct tcp_connection {
        
    private:
        struct tcp_client;
        struct tcp_server;
        tcp_client* client = nullptr;
        tcp_server* server = nullptr;
        proxy_server* parent;
        
    public:
        tcp_connection(client_socket* client, proxy_server* parent) : client(new tcp_client(client)), parent(parent) {};
        ~tcp_connection() { std::cout << "tcp_connect deleted\n"; };
        int get_client_sock() { return client->get_socket(); };
        int get_server_sock() { return server->get_socket(); };
        std::string get_host() { return client->host; }
        void set_addr(in_addr addr) { client->addr = addr; };
        void connect_to_server();
        void make_request();
        void server_handler(struct kevent event);
        void client_handler(struct kevent event);
        
        parse_state* state = nullptr;
        
    private:
        
        void add_request_text(std::string text);
        void read_request_f(struct kevent event);

        struct tcp_client {
            tcp_client(client_socket* client) : socket(client) {};
            ~tcp_client() {
                std::cout << socket->get_socket() << " client deleted\n";
                delete socket;
            };
            
            client_socket* socket;
            
            int get_socket() { return socket->get_socket(); };
            
            std::string request;
            std::string host;
            in_addr addr;
        };
        
        struct tcp_server {
            tcp_server(client_socket* socket) : socket(socket) {};
            ~tcp_server() {
                std::cout << socket->get_socket() << " server deleted\n";
                delete socket;
            };
            
            client_socket* socket;
            
            int get_socket() {
                return socket->get_socket();
            }
        };
    };
};




#endif /* proxy_hpp */

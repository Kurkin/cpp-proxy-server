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
    std::list<tcp_connection*> host_names;
    std::mutex queue_mutex;
    std::condition_variable queue_cond;
    bool queue_ready;
    
    std::mutex ans_mutex;
    std::list<tcp_connection*> ans;
    
    server_t* server;
    
    io_queue queue;
    
public:
    proxy_server(io_queue queue, int port);
    
    void resolve(tcp_connection* connection);
    
    std::function<void()> resolver = [&](){
        while (true) {
            std::unique_lock<std::mutex> lk(queue_mutex);
            queue_cond.wait(lk, [&]{return queue_ready;});
            
            tcp_connection* connection = host_names.front();
            host_names.pop_front();
            if (host_names.size() == 0) {
                queue_ready = false;
            }
            lk.unlock();
            
            if (!connection->alive) {
                continue;
            }
            
            std::string name = connection->get_host();
        
            if (name.find(":") != std::string::npos) {
                name.erase(name.find(":"));
            }
            
            struct hostent* addr;
            
            if ((addr = gethostbyname(name.c_str())) == NULL) {
                perror("resolve fail");
                std::cout << name << "\n";
                continue; //resolve error, just skip this host
            }
            
            connection->set_addr(*(in_addr*)addr->h_addr_list[0]);
            
            std::unique_lock<std::mutex> lk1(ans_mutex);
            if (connection->alive) {
                ans.push_back(connection);
            }
            lk1.unlock();
            
            queue.trigger_user_event_handler(USER_EVENT_IDENT);
            
            queue_cond.notify_one();
        }
    };
    
    tcp_connection* get_next() {
        std::unique_lock<std::mutex> lk1(ans_mutex);
        tcp_connection* connection = ans.front();
        ans.pop_front();
        return connection;
    }
    
    funct_t host_resolfed_f = [&](struct kevent event){

        tcp_connection* connection = get_next();
        connection -> connect_to_server();
        connection -> make_request();
        
        queue.add_event_handler(connection->get_server_sock(), EVFILT_READ, [connection](struct kevent event){
            connection->server_handler(event);
        });
        
        queue.add_event_handler(connection->get_client_sock(), EVFILT_READ, [connection](struct kevent event){
            connection->client_handler(event);
        });
        
        std::unique_lock<std::mutex> lk2(ans_mutex);
        if (ans.size() != 0) {
            queue.trigger_user_event_handler(USER_EVENT_IDENT);
        }
        
    };
    
    funct_t connect_client_f = [&](struct kevent event) {
        tcp_connection* connection = new tcp_connection(new client_t(server), this);
        std::cout << "client connected: " << connection->get_client_sock() << "\n";
        queue.add_event_handler(connection->get_client_sock(), EVFILT_READ, [connection](struct kevent event){
            connection->client_handler(event);
        });
    };
    
private:
    
    
    struct tcp_connection {
        
    private:
        struct tcp_client;
        struct tcp_server;
        tcp_client* client = nullptr;
        tcp_server* server = nullptr;
        proxy_server* parent;
        
    public:
        tcp_connection(client_t* client, proxy_server* parent) : client(new tcp_client(client)), parent(parent) {};
        ~tcp_connection() {};
        void set_server(client_t* client);
        int get_client_sock() { return client->get_socket(); };
        int get_server_sock() { return server->get_socket(); };
        std::string get_host() { return client->host; }
        void set_addr(in_addr addr) { client->addr = addr; };
        void connect_to_server();
        void make_request();
        void server_handler(struct kevent event);
        void client_handler(struct kevent event);
        void server_listener_f(struct kevent event) {
            if (event.flags & EV_EOF && event.data == 0) {
                delete server;
            } else {
                char buff[BUFF_SIZE];
                size_t size = read(get_client_sock(), buff, BUFF_SIZE);
                send(get_server_sock(), buff, size, 0);
            }
        }

        
        bool alive = true;
        
    private:
        
        void add_request_text(std::string text);
        void read_request_f(struct kevent event);

        struct tcp_client {
            tcp_client(client_t* client) : socket(client) {};
            ~tcp_client() { delete socket; };
            
            client_t* socket;
            
            int get_socket() { return socket->get_socket(); };
            
            std::string request;
            std::string host;
            in_addr addr;
        };
        
        struct tcp_server {
            tcp_server(client_t* socket) : socket(socket) {};
            ~tcp_server() { delete socket; };
            
            client_t* socket;
            
            int get_socket() {
                return socket->get_socket();
            }
        };
    };
};




#endif /* proxy_hpp */

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
    
    std::list<client_t*> host_names;
    std::mutex queue_mutex;
    std::condition_variable queue_cond;
    bool queue_ready;
    
    std::mutex ans_mutex;
    std::list<client_t*> ans;
    
    io_queue queue;
    int main_socket;
    char buff[BUFF_SIZE];
    std::map<int, std::string> requests;
    
    std::set<int> open_clients;
    std::set<int> open_servers;
    
    proxy_server(io_queue queue, int port);
    
    void put_in_host_names(client_t* request);
    
    std::function<void()> resolver = [&](){
        while (true) {
            std::unique_lock<std::mutex> lk(queue_mutex);
            queue_cond.wait(lk, [&]{return queue_ready;});
            
            client_t* client = host_names.front();
            host_names.pop_front();
            if (host_names.size() == 0) {
                queue_ready = false;
            }
            lk.unlock();
            
            std::string name = client->host;
        
            if (name.find(":") != std::string::npos) {
                name.erase(name.find(":"));
            }
            
            struct hostent* addr;
            
            if ((addr = gethostbyname(name.c_str())) == NULL) {
                perror("resolve fail");
                std::cout << name << "\n";
                continue; //resolve error, just skip this host
            }
            
            client->addr = *(in_addr*)addr->h_addr_list[0];
            
            std::unique_lock<std::mutex> lk1(ans_mutex);
            ans.push_back(client);
            lk1.unlock();
            
            queue.trigger_user_event_handler(USER_EVENT_IDENT);
            
            queue_cond.notify_one();
        }
    };
    
    funct_t host_resolfed_f = [&](struct kevent event){
        std::unique_lock<std::mutex> lk1(ans_mutex);
        client_t* client = ans.front();
        ans.pop_front();
        lk1.unlock();
        
        server_t* server = new server_t(client, &open_servers);
        
        client->server = server;
        
        std::cout << "connected with server on sock:" << server->socket << " client "<< client->socket << "\n";
        
        write(server->socket, client->request.c_str(), client->request.size());
        
        queue.add_event_handler(server->socket, EVFILT_READ, ex_ser, server);
        queue.add_event_handler(client->socket, EVFILT_READ, ex_cli, client);
        
        std::unique_lock<std::mutex> lk2(ans_mutex);
        if (ans.size() != 0) {
            queue.trigger_user_event_handler(USER_EVENT_IDENT);
        }
        lk2.unlock();
    };
    
    funct_t ex_ser = [&](struct kevent event) {
        server_t* server = (server_t*) event.udata;
        if (open_servers.find(static_cast<int>(event.ident)) != open_servers.end()) {
            server->server_listener_f(event);
        }
    };
    
    funct_t ex_cli = [&](struct kevent event) {
        client_t* client = (client_t*) event.udata;
        if (open_clients.find(static_cast<int>(event.ident)) != open_clients.end())
            if (client->client_listener_f(event) != NULL) {
                put_in_host_names(client);
            };
    };
    
    funct_t connect_client_f = [&](struct kevent event) {
        client_t* client = new client_t(main_socket, &open_clients);
        std::cout << "client connected, client discriptor: " << client->socket << "\n";
        queue.add_event_handler(client->socket, EVFILT_READ, ex_cli, client);
    };
    
};



#endif /* proxy_hpp */

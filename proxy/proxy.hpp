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
                    parse_ed->connection->set_client_addrinfo(cache.get(name + port));
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
            
            std::unique_lock<std::mutex> lk1(ans_mutex);
            std::unique_lock<std::mutex> lk2(state);
                if (!parse_ed->canceled) {
                    parse_ed->connection->set_client_addrinfo(*res);
                    cache.put(name + port, *res);
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
        
        if (connection->get_server_socket() != -1) {
            if (connection->request->get_host() == connection->host)
            {
                std::cout << "keep-alive is working!\n";
                //            try_to_cache();
                delete connection->response;
                connection->response = nullptr;
                connection->URI = connection->request->get_URI();
            } else {
                connection->deregistrate(connection->server);
            }
        }
        
        connection->server = tcp_client(client_socket(connection->client_addr));
        connection->host = connection->request->get_host();
        connection->URI = connection->request->get_URI();
        
        connection->set_server_on_read_write(
            [connection](struct kevent event)
            {
                if (event.flags & EV_EOF && event.data == 0) {  // TODO: check for errors!
                    std::cout << "EV_EOF from " << event.ident << " server\n";
//                    try_to_cache();
                    connection->deregistrate(connection->server);
                    connection->server = client_socket();
                } else {
                    char buff[event.data];
                    std::cout << "read from " << connection->get_server_socket() << "\n";
                    size_t size = recv(connection->get_server_socket(), buff, event.data, 0);
                    if (connection->response == nullptr) {
                        connection->response = new response({buff,size});
                    } else {
                        connection->response->add_part({buff, size});
                    }
                    connection->write_to_client({buff, size});
                }
            },
            [connection, this](struct kevent event)
            {
                if (connection->server.msg_queue.empty()) {
                    queue.delete_event_handler(event.ident, EVFILT_WRITE);
                    return;
                }
                write_part part = connection->server.msg_queue.front();
                connection->server.msg_queue.pop_front();
                std::cout << "write to " << event.ident << "\n";
                size_t writted = ::write(event.ident, part.get_part_text(), part.get_part_size());
                if (writted == -1) {
                    if (errno != EPIPE) {
                        throw_error(errno, "write()");
                    } else {
                        if (part.get_part_size() != 0) {
                            connection->server.msg_queue.push_front(part);
                        }
                    }
                } else {
                    part.writted += writted;
                    if (part.get_part_size() != 0) {
                        connection->server.msg_queue.push_front(part);
                    }
                }
            });
        
        connection->write_to_server(connection->request->get_request_text());
        delete connection->request;
        connection->request = nullptr;

        std::unique_lock<std::mutex> lk(ans_mutex);
        if (ans.size() != 0) {
            queue.trigger_user_event_handler(USER_EVENT_IDENT);
        }
    };
    
    funct_t connect_client = [this](struct kevent event) {
        proxy_tcp_connection* connection = new proxy_tcp_connection(queue, tcp_client(client_socket(server)));
        connection->set_client_on_read_write(
            [connection, this](struct kevent event)
            {
                if (event.flags & EV_EOF)
                {
                    std::cout << "EV_EOF from " << event.ident << " client\n";
                    delete connection;
                } else
                {
                    char buff[event.data];
                    
                    std::cout << "read request of " << event.ident << "\n";

                    size_t size = read(connection->get_client_socket() , buff, event.data);
                    if (size == static_cast<size_t>(-1)) {
                        throw_error(errno, "read()");
                    }
                    if (connection->request == nullptr) {
                        connection->request = new request({buff, size});
                    } else {
                        connection->request->add_part({buff, size});
                    }
                    if (connection->request->get_state() == BAD) {
//                    todo: non block write
//                        std::cout << "Bad Request\n";
//                        write(get_client_sock(), "HTTP/1.1 400 Bad Request\r\n\r\n");
                        delete connection;
                        return;
                    }
                    if (connection->request->get_state() == FULL_BODY) {
                        std::cout << "push to resolve " << connection->get_host() << connection->request->get_URI() << "\n";
                        if (connection->state) delete connection->state;
                        connection->state = new parse_state(connection);
                        resolve(connection->state);
                    }
                }
            },
            [connection, this](struct kevent event)
            {
                if (connection->client.msg_queue.empty()) {
                    queue.delete_event_handler(event.ident, EVFILT_WRITE);
                    return;
                }
                write_part part = connection->client.msg_queue.front();
                connection->client.msg_queue.pop_front();
                std::cout << "write to " << event.ident << "\n";
                size_t writted = ::write(event.ident, part.get_part_text(), part.get_part_size());
                if (writted == -1) {
                    if (errno != EPIPE) {
                        throw_error(errno, "write()");
                    } else {
                        if (part.get_part_size() != 0) {
                            connection->client.msg_queue.push_front(part);
                        }
                    }
                } else {
                    part.writted += writted;
                    if (part.get_part_size() != 0) {
                        connection->client.msg_queue.push_front(part);
                    }
                }
            });
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
        proxy_tcp_connection(io_queue& queue, tcp_client&& client);
        ~proxy_tcp_connection();
        
        std::string get_host() const noexcept;
        void set_client_addrinfo(addrinfo addr);
        void connect_to_server();
        
        struct request* request = nullptr;
        struct response* response = nullptr;
        parse_state* state = nullptr;
        std::string host;
        std::string URI;
        addrinfo client_addr;
    };

//        void make_request();
//        void try_to_cache();

};

#endif /* proxy_hpp */
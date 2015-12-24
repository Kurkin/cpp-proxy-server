//
//  socket.hpp
//  proxy
//
//  Created by Kurkin Dmitry on 18.12.15.
//
//

#ifndef socket_hpp
#define socket_hpp

#include <list>

#include "file_descriptor.h"
#include "kqueue.hpp"

struct server_socket
{
    server_socket(int port);
    
    int getfd() const noexcept { return fd.getfd(); };
    void bind_and_listen();
    
private:
    file_descriptor fd;
    int port;
};

struct client_socket
{
    client_socket() noexcept;
    client_socket(client_socket const& other) = delete;
    client_socket(client_socket&& other) noexcept;
    client_socket& operator=(client_socket&& rhs) noexcept;
    client_socket(const sockaddr addr); // for connect;
    client_socket(const server_socket& server); // for accept
    int getfd() const noexcept { return fd.getfd(); };
    
private:
    file_descriptor fd;
};

struct write_part {
    std::string text;
    size_t writted = 0;
    
    write_part(std::string text) : text(text) {};
    write_part(std::string text, size_t written) : text(text), writted(written) {};
    const char* get_part_text() const { return text.data() + writted; };
    size_t get_part_size() const { return text.size() - writted; }
};

struct tcp_client
{
    typedef std::function<void (struct kevent event)> on_ready_t;
    
    tcp_client();
    tcp_client(client_socket socket);
    tcp_client(client_socket socket, on_ready_t on_read, on_ready_t on_write);
    void set_on_read_write(on_ready_t on_read, on_ready_t on_write);
    int get_socket() const noexcept;
    
    on_ready_t on_read;
    on_ready_t on_write;
    client_socket socket;
    std::list<write_part> msg_queue;
};

struct tcp_connection
{
    typedef std::function<void (struct kevent event)> on_ready_t;
    
    tcp_connection(io_queue& queue, tcp_client client);
    ~tcp_connection() {
        deregistrate(client);
        if (get_server_socket() != -1)
            deregistrate(server);
    };
    void set_server(tcp_client server);
    void write_to_client(std::string text);
    void write_to_server(std::string text);
    int get_client_socket() const noexcept;
    int get_server_socket() const noexcept;
    void set_client_on_read_write(on_ready_t on_read, on_ready_t on_write);
    void set_server_on_read_write(on_ready_t on_read, on_ready_t on_write);
    void registrate(tcp_client& client);
    void deregistrate(tcp_client& client);
    void update_registration(tcp_client& client);
    
    io_queue& queue;

public:
    tcp_client client;
    tcp_client server;
};


#endif /* socket_hpp */

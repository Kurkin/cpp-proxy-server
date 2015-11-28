//
//  proxy.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "proxy.hpp"


void proxy_server::put_in_host_names(client_t* client) {
    std::lock_guard<std::mutex> lk(queue_mutex);
    host_names.push_back(client);
    queue_ready = true;
    queue_cond.notify_one();
}

proxy_server::proxy_server(io_queue queue, int port) {
    
    listen_socket sock;
    main_socket = sock.get_socket();
    
    if (sock.bind_and_listen(port) == -1) {
        errx(10, "init error");
    }
    
    this->queue = queue;
    
    queue.add_event_handler(main_socket, EVFILT_READ, connect_client_f);
    queue.add_event_handler(USER_EVENT_IDENT, EVFILT_USER, EV_CLEAR, host_resolfed_f);
}


//
//  dns_service.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "dns_service.hpp"

void resolve() {
    while (true) {
        std::unique_lock<std::mutex> lk(queue_mutex);
        queue_cond.wait(lk, [&]{return queue_ready;});
        
        Request* request = host_names.front();
        host_names.pop_front();
        if (host_names.size() == 0) {
            queue_ready = false;
        }
        lk.unlock();
        
        std::string name = request->headers.at("Host");
        
        struct hostent* addr;
            
        if ((addr = gethostbyname(name.c_str())) == NULL) {
            errx(1, "error");
        }
            
        request->addr_list = *(in_addr*)addr->h_addr_list[0];
            
        std::unique_lock<std::mutex> lk1(ans_mutex);
        ans.push_back(request);
        lk1.unlock();
            
        struct kevent user_trigger;
        EV_SET(&user_trigger, 0x5c0276ef, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
        queue_test.event_handler(user_trigger, NULL);
            queue_cond.notify_one();
        }
}
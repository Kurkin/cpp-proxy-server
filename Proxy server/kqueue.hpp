//
//  kqueue.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef kqueue_hpp
#define kqueue_hpp

#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>
#include <map>
#include <err.h>
#include <assert.h>

typedef std::function<void(struct kevent)> funct_t;

struct io_queue {
    
    io_queue();
    
    void add_event_handler(uintptr_t ident, int16_t filter, funct_t funct, void* udata = NULL);
    void add_event_handler(uintptr_t ident, int16_t filter, uint16_t flags, funct_t funct, void* udata = NULL);
    void delete_event_handler(uintptr_t ident, int16_t filter);
    void trigger_user_event_handler(uintptr_t ident);
    
    void watch_loop();
    
private:
    int ident;
};

void write(int dis, const char* message, size_t size);
#endif /* kqueue_hpp */

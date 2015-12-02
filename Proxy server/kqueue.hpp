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
#include "throw_error.h"

typedef std::function<void(struct kevent)> funct_t;

struct io_queue {
    
    io_queue();
    
    void add_event_handler(uintptr_t ident, int16_t filter, funct_t funct);
    void add_event_handler(uintptr_t ident, int16_t filter, uint16_t flags, funct_t funct);
    void delete_event_handler(uintptr_t ident, int16_t filter);
    void trigger_user_event_handler(uintptr_t ident);
    
    void watch_loop();
    void hard_stop(); //other
    void soft_stop(); //sigterm
    
private:
    int ident;
    bool ident_is_valid = true;
    bool finished = false;
};

void write(int dis, const char* message, size_t size);
#endif /* kqueue_hpp */

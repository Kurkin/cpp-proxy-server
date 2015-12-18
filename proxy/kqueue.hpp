//
//  kqueue.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef kqueue_hpp
#define kqueue_hpp

#include <sys/types.h>
#include <sys/event.h>
#include <map>
#include <vector>

#include "file_descriptor.h"
#include "timer.h"

typedef std::function<void(struct kevent)> funct_t;

struct io_queue {
    
    io_queue();
    io_queue(io_queue const&) = delete;
    
    void add_event_handler(uintptr_t ident, int16_t filter, funct_t funct);
    void add_event_handler(uintptr_t ident, int16_t filter, uint16_t flags, funct_t funct);
    void delete_event_handler(uintptr_t ident, int16_t filter);
    void trigger_user_event_handler(uintptr_t ident);
    
    void watch_loop();
    void hard_stop(); //other
    void soft_stop(); //sigterm
    
    timer& get_timer() noexcept;
    
private:
    int run_timers_calculate_timeout();
    
    std::map<std::pair<uintptr_t, int16_t>, funct_t> events_handlers;
    file_descriptor fd;
    std::vector<std::pair<uintptr_t, int16_t>> deleted_events;
    bool finished = false;
    struct timer timer;
};

#endif /* kqueue_hpp */

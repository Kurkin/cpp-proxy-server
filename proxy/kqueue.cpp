//
//  kqueue.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include <sys/socket.h>
#include <sys/errno.h>
#include <iostream>

#include "kqueue.hpp"
#include "throw_error.h"

io_queue::io_queue()
{
    fd = kqueue();
    if (fd.getfd() == -1) {
        throw_error(errno, "kqueue()");
    }
}

void io_queue::hard_stop() {
    finished = true;
}

void io_queue::add_event_handler(uintptr_t ident, int16_t filter, funct_t funct) {
    add_event_handler(ident, filter, 0, funct);
}

void io_queue::add_event_handler(uintptr_t ident, int16_t filter, uint16_t flags, funct_t funct) {
    struct kevent event;
    EV_SET(&event, ident, filter, EV_ADD|flags, 0, 0, NULL);
    int fail = kevent(fd.getfd(), &event, 1, NULL, 0, NULL);
    if (fail) {
        throw_error(errno, "kevent(EV_ADD)");
    }
    auto event_handler = std::pair<std::pair<uintptr_t, int16_t>, funct_t>
    (std::pair<uintptr_t, int16_t>(event.ident, event.filter),funct);
    events_handlers.erase(std::pair<uintptr_t, int16_t>(event.ident, event.filter));
    events_handlers.insert(event_handler);
}

void io_queue::delete_event_handler(uintptr_t ident, int16_t filter) {
    struct kevent event;
    EV_SET(&event, ident, filter, EV_DELETE, 0, 0, NULL);
    int fail = kevent(fd.getfd(), &event, 1, NULL, 0, NULL);
    if (fail == -1) {
        if (errno != ENOENT)
            throw_error(errno, "kevent(EV_DELETE)");
    }
    deleted_events.push_back({ident, filter});
    events_handlers.erase(std::pair<uintptr_t, int16_t>(ident, filter));
}

void io_queue::trigger_user_event_handler(uintptr_t ident) {
    struct kevent event;
    EV_SET(&event, ident, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
    int fail = kevent(fd.getfd(), &event, 1, NULL, 0, NULL);
    if (fail) {
        throw_error(errno, "kevent(NOTE_TRIGGER)");
    }
}

timer& io_queue::get_timer() noexcept
{
    return timer;
}

void io_queue::watch_loop() {
    size_t const evListSize = 256;
    struct kevent evList[evListSize];
    
    while (!finished)
    {
        int timeout = run_timers_calculate_timeout();
        struct timespec tmout = {timeout, 0};
        
        int new_events = kevent(fd.getfd(), NULL, 0, evList, evListSize, timeout == -1 ? nullptr : &tmout);
        if (new_events == -1) {
            switch (errno) {
                case EINTR:
                    // ignore;
                    break;
                default:
                    throw_error(errno, "kevent()");
                    break;
            }
            continue;
        }
        for (size_t i = 0; i < new_events && !finished; i++) {
            if (evList[i].ident != -1) {
                std::pair<uintptr_t, int16_t> event(evList[i].ident, evList[i].filter);
                if (events_handlers.find(event) != events_handlers.end()) {
                    funct_t funct = events_handlers.at(event);
                    funct(evList[i]);
                    if (deleted_events.size() != 0) {
                        for (size_t j = i; j < new_events; j++) {
                            for (size_t k = 0; k < deleted_events.size(); k++) {
                                if (std::make_pair(evList[j].ident, evList[j].filter) == deleted_events[k]) {
                                    evList[j].ident = -1;
                                }
                            }
                        }
                        while (!deleted_events.empty()) {
                            deleted_events.pop_back();
                        }
                    }
                }
            }
        }
    }
}

int io_queue::run_timers_calculate_timeout()
{
    if (timer.empty())
        return -1;
    
    timer::clock_t::time_point now = timer::clock_t::now();
    timer.notify(now);
    
    if (timer.empty())
        return -1;
    
    return std::chrono::duration_cast<std::chrono::milliseconds>(timer.top() - now).count();
}
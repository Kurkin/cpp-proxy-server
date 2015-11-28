//
//  kqueue.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "kqueue.hpp"

std::map<std::pair<uintptr_t, int16_t>, funct_t> events_handlers;

io_queue::io_queue() {
    ident = kqueue();
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
    int fail = kevent(this->ident, &event, 1, NULL, 0, NULL);
    if (fail) {
        perror("Adding in queue error");
        throw "Adding event handler in io_queue error";
    }
    auto event_handler = std::pair<std::pair<uintptr_t, int16_t>, funct_t>
    (std::pair<uintptr_t, int16_t>(event.ident, event.filter),funct);
    events_handlers.erase(std::pair<uintptr_t, int16_t>(event.ident, event.filter));
    events_handlers.insert(event_handler);
}

void io_queue::delete_event_handler(uintptr_t ident, int16_t filter) {
    struct kevent event;
    EV_SET(&event, ident, filter, EV_DELETE, 0, 0, NULL);
    int fail = kevent(this->ident, &event, 1, NULL, 0, NULL);
    if (fail) {
        perror("Deleting from queue error");
        throw "Deletng event handler from io_queue error";
    }
    events_handlers.erase(std::pair<uintptr_t, int16_t>(ident, filter));
}

void io_queue::trigger_user_event_handler(uintptr_t ident) {
    struct kevent event;
    EV_SET(&event, ident, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
    int fail = kevent(this->ident, &event, 1, NULL, 0, NULL);
    if (fail) {
        perror("Error in trigerring user event");
        throw "Error in trigerring user event";
    }
}

void io_queue::watch_loop() {
    size_t const evListSize = 256;
    struct kevent evList[evListSize];
    
    while (!finished) {
        int new_events = kevent(ident, NULL, 0, evList, evListSize, NULL);
        if (new_events == -1) {
            switch (errno) {
                case EINTR:
                    // ignore;
                    break;
                default:
                    perror("kqueue error");
                    errx(1, "kqueue error");
                    break;
            }
            continue;
        }
        
//        std::cout << "in cycle\n";
        
        for (size_t i = 0; i < new_events && !finished; i++) {
            std::pair<uintptr_t, int16_t> event(evList[i].ident, evList[i].filter);
            if (events_handlers.find(event) != events_handlers.end()) {
                funct_t funct = events_handlers.at(event);
                funct(evList[i]);
            }
        }
    }
}

void write(int dis, const char* message, size_t size) {
    send(dis, message, size, 0);
}
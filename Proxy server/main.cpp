//  main.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>
#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include <set>
#include <assert.h>

#include "kqueue.hpp"
#include "http_parser.hpp"
#include "proxy.hpp"

int main(int argc, const char * argv[]) {
  
    io_queue queue;
    
    proxy_server proxy(queue, 2540);
    
    std::thread dns_resolver1(proxy.resolver);
    std::thread dns_resolver2(proxy.resolver);
    std::thread dns_resolver3(proxy.resolver);
    std::thread dns_resolver4(proxy.resolver);
    
    queue.watch_loop();
    
    return 0;
}


// TODO:
// 1) save_write (sigpipe problem)/ save_read
// 2) bug: serever close wrong client | fixed
// 3) bug: client have 2 and more servers | fixed
// 4) bug: server is alive then client is dead | fixed
// 5) catch sig(SIGTERM) in io_queue
// 6) change signature of event_handler (create kevent in io_queue) (optional)
// 7) make a copy of request in Request 


//  main.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include <thread>

#include "kqueue.hpp"
#include "proxy.hpp"

int main(int argc, const char * argv[])
{  
    io_queue queue;
    
    proxy_server proxy(queue, 2540);
    
    std::thread dns_resolver1(proxy.resolver);
    std::thread dns_resolver2(proxy.resolver);
    
    queue.watch_loop();
    
    return 0;
}
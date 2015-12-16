//  main.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include <thread>
#include <signal.h>
#include <iostream>

#include "kqueue.hpp"
#include "proxy.hpp"

io_queue queue;

int main()
{  

    signal(SIGTERM, [](int sig){ queue.hard_stop(); });
    
    proxy_server proxy(queue, 2540);
    
    std::thread dns_resolver1(proxy.resolver);
    std::thread dns_resolver2(proxy.resolver);

//    try {
        queue.watch_loop();
//    } catch (std::runtime_error error){
//        std::cout << error.what() << "\n";
//    }

    return 0;
}

//  main.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include <signal.h>
#include <iostream>

#include "kqueue.hpp"
#include "proxy.hpp"

io_queue queue; // todo signal to queue

int main()
{
 
    signal(SIGTERM, [](int sig){ queue.hard_stop(); });
    
    proxy_server proxy(queue, 2540);
    
//    try {
        queue.watch_loop();
//    } catch (std::runtime_error error){
//        std::cout << error.what() << "\n";
//    }

    return 0;
}

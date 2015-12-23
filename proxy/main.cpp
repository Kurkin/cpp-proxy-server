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

int main()
{
    try {
        io_queue queue;
        proxy_server proxy(queue, 2540, 2);
        queue.watch_loop();
    } catch (std::runtime_error error) {
        std::cout << error.what() << "\n";
    }

    return 0;
}

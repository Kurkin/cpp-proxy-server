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
#include "DNSresolver.hpp"

int main()
{
    try {
        io_queue queue;
        DNSresolver resolver(2);
        proxy_server proxy(queue, 2540, resolver);
        queue.watch_loop();
    } catch (std::runtime_error const& error) {
        std::cout << error.what() << "\n";
    }

    return 0;
}

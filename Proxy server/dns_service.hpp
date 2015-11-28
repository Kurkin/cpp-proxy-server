//
//  dns_service.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 21.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef dns_service_hpp
#define dns_service_hpp

#include <mutex>
#include <list>
#include "http_parser.hpp"

std::list<Request*> host_names;
std::mutex queue_mutex;
std::condition_variable queue_cond;
bool queue_ready;

std::mutex ans_mutex;
std::list<Request*> ans;

void resolve();

#include <stdio.h>

#endif /* dns_service_hpp */

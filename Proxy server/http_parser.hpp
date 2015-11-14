//
//  http_parser.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 12.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef http_parser_hpp
#define http_parser_hpp

#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>
#include <err.h>

struct request {
    struct in_addr **addr_list;
    std::string host_name;
    std::string mes;
    
    request(char* message, size_t length);
};

int drop(std::string substr, std::string* string);

#endif /* http_parser_hpp */

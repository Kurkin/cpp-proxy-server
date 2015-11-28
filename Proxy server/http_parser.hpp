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
#include <map>
#include <string.h>
#include <assert.h>

//typedef std::pair<std::string::iterator, std::string::iterator> sub_string;

struct Request {
    
//    sub_string method1;
//    sub_string URI2;
//    sub_string version2;
    std::string text;
    
    std::string method;
    std::string URI;
    std::string version;
    std::string body;
    std::map<std::string, std::string> headers;
    int client;
    
    in_addr addr_list; //  TODO: make a list
        
    Request(char* message, size_t length, int client);

    std::string get_request();
};

#endif /* http_parser_hpp */

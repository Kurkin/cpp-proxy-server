//
//  http_handler.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 02.12.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef http_handler_hpp
#define http_handler_hpp

#include <stdio.h>
#include <iostream>
#include <map>

struct http
{
    typedef std::pair<std::string::iterator, std::string::iterator> substr;
    
    http(std::string text);
    
    std::string const get_header(std::string name);
    std::string const get_body() { return {body.first, body.second}; };  // bad alloc
    
private:
    std::string text;
    std::map<std::string, substr> headers;
    substr body;
};

struct request : public http
{
    request(std::string);

    std::string const get_method() { return {method.first, method.second}; };
    
private:
    substr method;
    substr URI;
    substr http_version;
};

struct response : public http
{
    response(std::string);
    
private:
    substr http_version;
    substr code;
    substr code_transcript;
};

#endif /* http_handler_hpp */

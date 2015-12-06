//
//  http_handler.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 02.12.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef http_handler_hpp
#define http_handler_hpp

#include <iostream>
#include <map>

struct http
{
//    typedef std::pair<std::string::iterator, std::string::iterator> substr; its dont work
    
    http(std::string text);
    
    std::string get_header(std::string name) const;
    std::string get_body() const { return body; }
private:
    std::string text;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct request : public http
{
    request(std::string);

    std::string get_method() const { return method; }
    std::string get_URI() const { return URI; }
    std::string make_request() const;
private:
    std::string method;
    std::string URI;
    std::string http_version;
    std::string other;
};

struct response : public http
{
    response(std::string);
    
    std::string get_code() const { return code; }
    std::string make_redirect_response() const;
    std::string make_cache_response() const;
private:
    std::string http_version;
    std::string code;
    std::string code_transcript;
};

#endif /* http_handler_hpp */

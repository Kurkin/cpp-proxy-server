//
//  http_handler.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 02.12.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "http_handler.hpp"

#define CR "\r"
#define LF "\n"
#define CRLF "\r\n"

// \r\n

http::http(std::string text) : text(text)
{
    auto headers_start = std::find_if(text.begin(), text.end(), [](char a) {
        return a == '\n';
    })++;
    auto headers_end = headers_start + 1;
    
    while (headers_end != text.end() && *headers_end != '\r')
    {
        auto space = std::find_if(headers_end, text.end(), [](char a) { return a == ':'; });
        auto crlf = std::find_if(space + 1, text.end(), [](char a){ return a == '\r'; }); // todo: rename
        
        headers.insert(std::make_pair(std::string(headers_end, space), std::make_pair(space + 2, crlf)));
        headers_end = crlf + 2;
    };
    
    if (headers_end + 2 != text.end()) {
        body = std::make_pair(headers_end + 2, text.end());
    } else {
        body = std::make_pair(text.end(), text.end());
    }
}

std::string const http::get_header(std::string name) {
    if (headers.find(name) != headers.end()) {
        auto value = headers.at(name);
        std::cout << "get " << name << " ans is " << std::string(value.first, value.second) << "\n";
        return {value.first, value.second};
    }
    return "";
}

request::request(std::string text) : http(text)
{
    auto first_space = std::find_if(text.begin(), text.end(), [](char a) { return a == ' '; });
    auto second_space = std::find_if(first_space + 1, text.end(), [](char a) { return a == ' '; });
    auto r_n = std::find_if(second_space + 1, text.end(), [](char a) { return a == '\r'; });
    
    method = std::make_pair(text.begin(), first_space);
    URI = std::make_pair(first_space + 1, second_space);
    http_version = std::make_pair(second_space + 1, r_n);
}

response::response(std::string text) : http(text)
{
    auto first_space = std::find_if(text.begin(), text.end(), [](char a) { return a == ' '; });
    auto second_space = std::find_if(first_space + 1, text.end(), [](char a) { return a == ' '; });
    auto r_n = std::find_if(second_space + 1, text.end(), [](char a) { return a == '\r'; });
    
    http_version = std::make_pair(text.begin(), first_space);
    code = std::make_pair(first_space + 1, second_space);
    code_transcript = std::make_pair(second_space + 1, r_n);
}
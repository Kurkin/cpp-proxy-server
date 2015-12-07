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
        auto crlf = std::find_if(space + 1, text.end(), [](char a){ return a == '\r'; });

        headers.insert({{headers_end, space}, {space + 2, crlf}});
        headers_end = crlf + 2;
    };

    if (headers_end + 2 != text.end()) {
        body = {headers_end + 2, text.end()};
    } else {
        body = {text.end(), text.end()};
    }
}

std::string http::get_header(std::string name) const
{
    if (headers.find(name) != headers.end()) {
        auto value = headers.at(name);
        return value;
    }
    return "";
}

request::request(std::string text) : http(text)
{
    auto first_space = std::find_if(text.begin(), text.end(), [](char a) { return a == ' '; });
    auto second_space = std::find_if(first_space + 1, text.end(), [](char a) { return a == ' '; });
    auto crlf = std::find_if(second_space + 1, text.end(), [](char a) { return a == '\r'; });

    method = {text.begin(), first_space};
    URI = {first_space + 1, second_space};
    http_version = {second_space + 1, crlf};
    other = {crlf + 2, text.end()};
}

std::string request::make_request() const {
    std::string host = get_header("Host");
    if (URI.find(host) != -1) {
        return method + " " + URI.substr(URI.find(host) + host.size()) + " " + http_version + "\r\n" + other;
    } else {
        return method + " " + URI + " " + http_version + "\r\n" + other;
    }
}

response::response(std::string text) : http(text)
{
    auto first_space = std::find_if(text.begin(), text.end(), [](char a) { return a == ' '; });
    auto second_space = std::find_if(first_space + 1, text.end(), [](char a) { return a == ' '; });
    auto crlf = std::find_if(second_space + 1, text.end(), [](char a) { return a == '\r'; });

    http_version = {text.begin(), first_space};
    code  = {first_space + 1, second_space};
    code_transcript = {second_space + 1, crlf};
}

std::string response::make_redirect_response() const
{
    return http_version + " " + code + " " + code_transcript +
    "\r\nContent-length: 0\r\nLocation: " + get_header("Location") + "\r\n\r\n";
}

std::string response::make_cache_response() const
{
    return http_version + " " + code + " " + code_transcript +
    "\r\nContent-Length:" + get_header("Content-Length") +
    "\r\nETag: " + get_header("ETag") + "\r\n\r\n" + get_body();
}
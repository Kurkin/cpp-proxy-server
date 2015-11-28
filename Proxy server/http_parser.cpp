//
//  http_parser.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 12.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "http_parser.hpp"

Request::Request(char* message, size_t length, int client): text(message, length), client(client) {
    
//    auto end = std::find_if(text.begin(), text.end(), [](char a) {
//        return a == '\n' || a == '\r' || a == ' ';
//    });
//    
//    method1 = std::make_pair(text.begin(), end);
//    if (std::string(method1.first, method1.second) != "POST" || std::string(method1.first, method1.second) != "GET") {
//        throw "Trash in request";
//    }
    
    body = text.substr(text.find("\r\n\r\n") + 4);
    
    std::cerr << body << "\n";
    
    method = strtok(message, "\n\r ");
    URI = strtok(NULL, "\n\r ");
    version = strtok(NULL, "\n\r ");
    char* line = strtok(NULL, "\n\r");
    while (line != NULL) {
        std::string l(line);
        size_t pos;
        std::string name;
        if ((pos = l.find(": ")) != std::string::npos) {
            name = l.substr(0, pos);
            l.erase(0, pos + 2);
            std::string value = l;
            headers.insert(std::pair<std::string, std::string>(name, value));
        }
        line = strtok(NULL, "\n\r");
    }
    
    if (headers.find("Content-Length") != headers.end()) {
        std::cout << body.length() << " " << std::stoi(headers.at("Content-Length")) << "\n";
        assert(body.length() == std::stoi(headers.at("Content-Length")));
    }
}

std::string Request::get_request() {
    return text;
}
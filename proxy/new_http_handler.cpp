//
//  new_http_handler.cpp
//  proxy
//
//  Created by Kurkin Dmitry on 08.12.15.
//
//

#include "new_http_handler.hpp"


std::string request::get_request_text() {  // todo: drop hop-by-hop headers
    return method + " " + URI.substr(URI.find(host) + host.size()) + " " + http_version + "\r\n"
        + std::string(std::find_if(text.begin(), text.end(), [](char a) { return a == '\r'; }) + 2, text.end());
}

request::request(std::string text) : text(text) {
    update_state();
}

void request::update_state() {
    if (state == 0 && text.find("\r\n") != std::string::npos) {
        state = FIRST_LINE;
        parse_request_line();
    }
    if (state == FIRST_LINE && (body_start == 0 || body_start == std::string::npos)) {
        body_start = text.find("\r\n\r\n");
    }
    if (state == FIRST_LINE && body_start != std::string::npos && body_start != 0) {
        state = FULL_HEADERS;
        body_start = parse_headers();
        host = headers.at("Host");
        if (host == "") {
            throw std::runtime_error("emty host");
        }
        check_body();
    }
}

void request::parse_request_line() {
    auto first_space = std::find_if(text.begin(), text.end(), [](char a) { return a == ' '; });
    auto second_space = std::find_if(first_space + 1, text.end(), [](char a) { return a == ' '; });
    auto crlf = std::find_if(second_space + 1, text.end(), [](char a) { return a == '\r'; });
        
    method = {text.begin(), first_space};
    URI = {first_space + 1, second_space};
    http_version = {second_space + 1, crlf};
}

int request::parse_headers() {
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
    
    return std::distance(text.begin(), headers_end + 2);
}

std::string request::get_header(std::string name) const
{
    if (headers.find(name) != headers.end()) {
        auto value = headers.at(name);
        return value;
    }
    return "";
}

void request::check_body() {
    body = text.substr(body_start);
    
    if (get_header("Content-Length") != "")
    {
        std::cout << body.size() << " " << std::stoi(get_header("Content-Length")) << "\n";
        if (body.size() == static_cast<size_t>(std::stoi(get_header("Content-Length")))) {
            state = FULL_BODY;
        } else {
            state = PARTICAL_BODY;
        }
    }
    else if (get_header("Transfer-Encoding") == "chunked")
    {
        if (std::string(body.end() - 7, body.end()) == "\r\n0\r\n\r\n") {
            state = FULL_BODY;
        } else {
            state = PARTICAL_BODY;
        }
    }
    else if (body.size() == 0)
    {
        state = FULL_BODY;
    }
}

int request::add_part(std::string part) {
    text.append(part);
    update_state();
    return get_state();
}


//std::string response::get_request_text() {  // todo: drop hop-by-hop headers
//    return method + " " + URI.substr(URI.find(host) + host.size()) + " " + http_version + "\r\n"
//    + std::string(std::find_if(text.begin(), text.end(), [](char a) { return a == '\r'; }) + 2, text.end());
//}


void response::update_state() {
    if (state == 0 && text.find("\r\n") != std::string::npos) {
        state = FIRST_LINE;
        parse_response_line();
    }
    if (body_start == 0 || body_start == std::string::npos) {
        body_start = text.find("\r\n\r\n");
    }
    if (state == FIRST_LINE && body_start != std::string::npos) {
        state = FULL_HEADERS;
        body_start = parse_headers();
        check_body();
    }
}

void response::parse_response_line() {
    auto first_space = std::find_if(text.begin(), text.end(), [](char a) { return a == ' '; });
    auto second_space = std::find_if(first_space + 1, text.end(), [](char a) { return a == ' '; });
    auto crlf = std::find_if(second_space + 1, text.end(), [](char a) { return a == '\r'; });
    
    http_version = {text.begin(), first_space};
    code = {first_space + 1, second_space};
    code_description = {second_space + 1, crlf};
}

int response::parse_headers() {
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
    
    return std::distance(text.begin(), headers_end + 2);
}

std::string response::get_header(std::string name) const
{
    if (headers.find(name) != headers.end()) {
        auto value = headers.at(name);
        return value;
    }
    return "";
}

void response::check_body() {
    body = text.substr(body_start);
    
    if (get_header("Content-Length") != "")
    {
        if (text.length() == static_cast<size_t>(std::stoi(get_header("Content-Length")))) {
            state = FULL_BODY;
        } else {
            state = PARTICAL_BODY;
        }
    }
    else if (get_header("Transfer-Encoding") == "chunked")
    {
        if (std::string(text.end() - 7, text.end()) == "\r\n0\r\n\r\n") {
            state = FULL_BODY;
        } else {
            state = PARTICAL_BODY;
        }
    }
    else
    {
        state = FULL_BODY;
    }
}
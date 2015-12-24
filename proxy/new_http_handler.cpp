//
//  new_http_handler.cpp
//  proxy
//
//  Created by Kurkin Dmitry on 08.12.15.
//
//

#include "new_http_handler.hpp"
#include <algorithm>

http::~http() {}

void http::add_part(std::string&& part)
{
    text.append(part);
    update_state();
}

void http::update_state()
{
    if (state == 0 && text.find("\r\n") != std::string::npos) {
        state = FIRST_LINE;
        parse_first_line();
    }
    if (state == FIRST_LINE && (body_start == 0 || body_start == std::string::npos)) {
        body_start = text.find("\r\n\r\n");
    }
    if (state == FIRST_LINE && body_start != std::string::npos && body_start != 0) {
        state = FULL_HEADERS;
        body_start += 4;
        parse_headers();
    }
    if (state >= FULL_HEADERS) {
        check_body();
    }
}

void http::parse_headers()
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
}

std::string http::get_header(std::string&& name) const
{
    if (headers.find(name) != headers.end()) {
        auto value = headers.at(name);
        return value;
    }
    return "";
}

void http::check_body()
{
    size_t length = text.size() - body_start;
    
    if (get_header("Content-Length") != "")
    {
        if (length == static_cast<size_t>(std::stoi(get_header("Content-Length")))) {
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
    else if (length == 0)
    {
        state = FULL_BODY;
    } else {
        state = BAD;
    }
}

std::string request::get_URI()
{
    if (URI.find(host) != -1)  // bug!!
        URI = URI.substr(URI.find(host) + host.size());
    return URI;
}

std::string request::get_host()
{
    if (method == "CONNECT")
        return URI;
    if (host == "")
        host = get_header("Host");
    if (host == "")
        host = get_header("host");
    if (host == "")
        throw std::runtime_error("empty host");
    return host;
}

void request::parse_first_line()
{
    auto first_space = std::find(text.begin(), text.end(), ' ');
    auto second_space = std::find_if(first_space + 1, text.end(), [](char a) { return a == ' '; });
    auto crlf = std::find_if(second_space + 1, text.end(), [](char a) { return a == '\r'; });
    
    if (first_space == text.end() || second_space == text.end() || crlf == text.end()) {
        state = BAD;
        return;
    }
    
    method = {text.begin(), first_space};
    URI = {first_space + 1, second_space};
    http_version = {second_space + 1, crlf};
    
    if (URI == "") {
        state = BAD;
        return;
    }
    if (http_version != "HTTP/1.1" && http_version != "HTTP/1.0") {
        state = BAD;
        return;
    }
}

std::string request::get_request_text() const
{
    std::string first_line;
    if (method == "CONNECT") {
        first_line = method + " " + host + URI + " " + http_version + "\r\n";
    } else {
        first_line = method + " " + URI + " " + http_version + "\r\n";
    }
    std::string headers = "";
    for (auto it : this->headers) {
        if (it.first != "Proxy-Connection")
            headers.append(it.first + ": " + it.second + "\r\n"); // todo: drop hop-by-hop headers
    }
    headers += "\r\n";
    
    return first_line + headers + text.substr(body_start);
}

bool request::is_validating() const
{
    return  get_header("If-Match") != ""
            || get_header("If-Modified-Since") != ""
            || get_header("If-None-Match") != ""
            || get_header("If-Range") != ""
            || get_header("If-Unmodified-Since") != "";
}

bool response::is_cacheable() const
{
    return state == FULL_BODY
           && get_header("ETag") != ""
           && get_header("Vary") == ""
           && get_code() == "200";
}

request* response::get_validating_request(std::string URI, std::string host) const
{
    return new request{"GET " + URI + " HTTP/1.1\r\nIf-None-Match: " + get_header("ETag") + "\r\nHost: " + host + "\r\n\r\n"};
}

void response::parse_first_line()
{
    auto first_space = std::find_if(text.begin(), text.end(), [](char a) { return a == ' '; });
    auto second_space = std::find_if(first_space + 1, text.end(), [](char a) { return a == ' '; });
    auto crlf = std::find_if(second_space + 1, text.end(), [](char a) { return a == '\r'; });
    
    if (first_space == text.end() || second_space == text.end() || crlf == text.end()) {
        state = BAD;
        return;
    }
    
    http_version = {text.begin(), first_space};
    code = {first_space + 1, second_space};
    code_description = {second_space + 1, crlf};
    
    if (http_version != "HTTP/1.1" && http_version != "HTTP/1.0") {
        state = BAD;
        return;
    }
}
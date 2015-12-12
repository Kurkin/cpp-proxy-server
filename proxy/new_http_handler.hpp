//
//  new_http_handler.hpp
//  proxy
//
//  Created by Kurkin Dmitry on 08.12.15.
//
//

#ifndef new_http_handler_hpp
#define new_http_handler_hpp

#define BAD -1
#define FIRST_LINE 1 // first line get, headers not full
#define FULL_HEADERS 2 // headers get and request host ready to resolve
#define PARTICAL_BODY 3
#define FULL_BODY 4

#include <iostream>
#include <map>

struct request
{
    request(std::string);
    int add_part(std::string);
    
    std::string get_method() const { return method; }
    std::string get_URI() const { return URI; }
    std::string get_header(std::string) const;
    std::string get_body() const { return body; }
    std::string get_host() const { return host; }
    int get_state() { return state; };
    std::string get_request_text();
    
private:
    void update_state();
    void check_body();
    int parse_headers();
    void parse_request_line();
    
    int state = 0;
    size_t body_start = 0;
    std::string text;
    std::string method;
    std::string URI;
    std::string http_version;
    std::string host;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct response {
    response(std::string text) : text(text) { update_state(); }
    void add_part(std::string part) {
        text.append(part);
    }
    int get_state() {
        update_state();
        return state;
    };
    std::string get_header(std::string) const;
    
private:
    void update_state();
    void check_body();
    int parse_headers();
    void parse_response_line();
    
    int state = 0;
    size_t body_start = 0;
    std::string text;
    std::string code;
    std::string code_description;
    std::string http_version;
    std::string body;
    std::map<std::string, std::string> headers;
};

#endif /* new_http_handler_hpp */

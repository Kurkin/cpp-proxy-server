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
#include <string>

struct http
{
    http(std::string&& text) : text(text) {}
    virtual ~http() = 0;
    void add_part(std::string&&);
    
    int get_state() { return state; };
    std::string get_header(std::string&&) const;
    std::string get_body() const { return text.substr(body_start); }
    std::string get_text() const { return text; }
    
protected:
    void update_state();
    void check_body();
    void parse_headers();
    virtual void parse_first_line() = 0;

    int state = 0;
    size_t body_start = 0;
    std::string text;
    std::map<std::string, std::string> headers;
};

struct request : public http
{
    request(std::string&& text) : http(std::move(text)) { update_state(); };
    
    std::string get_method() const { return method; }
    std::string get_URI();
    std::string get_host();
    std::string get_request_text() const;
    
    bool is_validating() const;
    
private:
    void parse_first_line() override;

    std::string method;
    std::string URI;
    std::string http_version;
    std::string host = "";
};

struct response : public http
{
    response(std::string&& text) : http(std::move(text)) { update_state(); };
    bool is_cacheable() const;
    std::string get_code() const { return code; }
    request* get_validating_request(std::string URI, std::string host) const;
    
private:
    void parse_first_line() override;
    
    std::string code;
    std::string code_description;
    std::string http_version;
};

#endif /* new_http_handler_hpp */

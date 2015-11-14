//
//  http_parser.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 12.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include "http_parser.hpp"

request::request(char* message, size_t length) {
    mes = std::string(message, length);
    size_t host = mes.find("Host:") + 6;
    size_t endhost = mes.find("\n", host);
    host_name = mes.substr(host, endhost - host - 1);
    printf("HOST NAME IS %s\n", host_name.c_str());
    
//  Drop: Proxy-Connection: keep=alive
    drop("Proxy-Connection: keep-alive", &mes);
//  DROP:  Connection: keep-alive
    drop("Connection: keep-alive", &mes);
    mes.resize(mes.size() - 2);
    
    mes += "Connection: close\n\n";
    
    const char* address = host_name.c_str();
    
    printf("resolving %s\n", address);
    
    struct hostent *addr;
    
    if ((addr = gethostbyname(address)) == NULL) {
        errx(1, "error");
    }
    
    printf("IP addresses: ");
    addr_list = (struct in_addr **)addr->h_addr_list;
    for(size_t i = 0; addr_list[i] != NULL; i++) {
        printf("%s ", inet_ntoa(*addr_list[i]));
    }
    printf("\n");
}


int drop(std::string substr, std::string* string) {
    size_t start = string->find(substr);
    if (start == std::string::npos) {
        return -1;
    }
    size_t end = string->find("\n", start);
    string = &string->erase(start, end - start + 1);
    return 0;
}

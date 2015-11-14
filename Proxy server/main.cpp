//
//  main.cpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 31.10.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/time.h>
//#include <signal.h>
#include <arpa/inet.h>
#include "kqueue.hpp"
#include "http_parser.hpp"

//bool quit = false;

//void takesig(int sig) {
//    quit = true;
//}

//TODO: catch SIGTERM

int main(int argc, const char * argv[]) {
    
//    server echo_server(2540);
    
//    signal(SIGTERM, takesig);
//    
//    echo_server.add_connect_function([](int d){
//        auto buff = "welcome to echo_server\n";
//        write(d, buff, strlen(buff));
//    });
//    
//    echo_server.add_recv_function([](int d){
//        char buff[256];
//        size_t size = read(d, buff);
//        write(d, buff, size);
//    });
//    echo_server.watch_loop();
    
    server getHttpAns(2539);
    
    getHttpAns.add_recv_function([](int d){
        char buff[1024];
        size_t size1 = read(d, buff);
        std::string request_from_client(buff, size1);
        std::cout << "message:\n";
        std::cout << request_from_client << "\n";
        
        request req(buff, size1);
        
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            perror("cant open sock");
        }
        std::cout << "sock opened\n";
        sockaddr_in server;
        server.sin_family = AF_INET;
        std::string address = inet_ntoa(*req.addr_list[0]);
        if (req.host_name.compare("notify2.dropbox.com") == 0) {
            close(sock);
            return;
        }
        server.sin_addr.s_addr = inet_addr(address.c_str());
        server.sin_port = htons(80);
        std::cout << "try to connect to " << address << "\n";
        if (connect(sock,  (struct sockaddr *)&server, sizeof(server)) < 0) {
            perror("cant connect");
            return;
        }
        std::cout << "connected\n\n";
        
        std::cout << req.mes << "\n";
        
        write(sock, req.mes.c_str(), strlen(req.mes.c_str()));
        std::cout << "request\n";
        size_t s = 1;
        while (s > 0) {
            s = read(sock, buff);
            std::cout << s << "\n";
            write(d, buff, s);
            std::cout << s << " !\n";
        }
        close(sock);
        std::cout << "sock closed\n";
    });
    
    getHttpAns.watch_loop();
    
    return 0;
}

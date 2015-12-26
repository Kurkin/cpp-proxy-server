//
//  DNSresolver.cpp
//  proxy
//
//  Created by Kurkin Dmitry on 24.12.15.
//
//

#include "DNSresolver.hpp"

#include <netdb.h>
#include <iostream>

DNSresolver::DNSresolver() : DNSresolver(2)
{}

DNSresolver::DNSresolver(size_t thread_count) : addr_cache(10000)
{
    for (size_t i = 0; i < thread_count; i++)
        resolvers.push_back(std::thread(&DNSresolver::resolver, this));
}

DNSresolver::~DNSresolver()
{
    finished = true;
    condition.notify_all();
    for (auto& thread : resolvers)
        thread.join();
}

void DNSresolver::resolver()
{
    while (!finished) {
        std::unique_lock<std::mutex> lk(main_mutex);
        condition.wait(lk, [&]{return !resolve_queue.empty() || finished;});

        if (finished)
            break;
        
        auto request = std::move(resolve_queue.front());
        resolve_queue.pop_front();
        lk.unlock();
        
        std::string port = "80";
        if (request->hostname.find(":") != static_cast<size_t>(-1)) {
            size_t port_str = request->hostname.find(":");
            port = request->hostname.substr(port_str + 1);
            request->hostname = request->hostname.erase(port_str);
        }
    
        sockaddr resolved;
        
        std::unique_lock<std::mutex> lk2(cache_mutex);
        if (addr_cache.contain(request->hostname + port)) {
            resolved = addr_cache.get(request->hostname + port);
            lk2.unlock();
        } else {
            lk2.unlock();
            struct addrinfo hints, *res;
            
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_NUMERICSERV;
            
            int error = getaddrinfo(request->hostname.c_str(), port.c_str(), &hints, &res);
            if (error) {
                // todo: do something on error
                perror(gai_strerror(error));
                continue;
            }
            
            resolved = *(res->ai_addr);
            freeaddrinfo(res);
            
            addr_cache.put(request->hostname + port, resolved);
        }
        
        std::unique_lock<std::mutex> lk1(request->state_mutex);
        if (!request->canceled) {
            request->callback(resolved);
        }
        
        condition.notify_one();
    }
}

DNSresolver::request::request(std::string const& hostname, callback_t callback) : callback( std::move(callback)), hostname(hostname)
{};

resolve_state DNSresolver::resolve(const std::string &host, callback_t callback)
{
    std::unique_ptr<request> req(new request(host, std::move(callback)));
    auto temp = req.get();
    resolve_queue.push_back(temp);
    condition.notify_one();
    return resolve_state(std::move(req));
}

resolve_state::resolve_state() {}

resolve_state::resolve_state(std::unique_ptr<DNSresolver::request> request)
{
    this->request = std::move(request);
}

resolve_state::resolve_state(resolve_state&& other)
{
    this->request = std::move(other.request);
}

resolve_state::~resolve_state()
{}

resolve_state& resolve_state::operator=(resolve_state &&other)
{
    std::swap(this->request, other.request);
    return *this;
}

void resolve_state::cancel()
{
    std::unique_lock<std::mutex> lk(request->state_mutex);
    request->canceled = true;
}

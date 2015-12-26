//
//  DNSresolver.hpp
//  proxy
//
//  Created by Kurkin Dmitry on 24.12.15.
//
//

#ifndef DNSresolver_hpp
#define DNSresolver_hpp

#include <mutex>
#include <vector>
#include <deque>
#include <thread>
#include <condition_variable>
#include <string>

#include "utils.hpp"

typedef std::function<void(struct sockaddr)> callback_t;

struct resolve_state;

struct DNSresolver
{
    DNSresolver();
    DNSresolver(size_t thread_count);
    ~DNSresolver();

    resolve_state resolve(std::string const& host, callback_t callback);
    void resolver();
    
private:
    struct request
    {
        request(std::string const& hostname, callback_t callback);
        bool canceled = false;
        callback_t callback;
        std::string hostname;
        std::mutex state_mutex;
    };
    
    std::mutex cache_mutex;
    lru_cache<std::string, sockaddr> addr_cache;
    std::vector<std::thread> resolvers;
    std::deque<request*> resolve_queue;
    std::mutex main_mutex;
    bool finished = false;
    std::condition_variable condition;
    
    friend resolve_state;
};

struct resolve_state
{
    resolve_state();
    resolve_state(std::unique_ptr<DNSresolver::request> request);
    resolve_state(resolve_state const& other) = delete;
    resolve_state(resolve_state&& other);
    resolve_state& operator=(resolve_state&& other);
    ~resolve_state();
    
    void cancel();
    
private:
    std::unique_ptr<DNSresolver::request> request;
};


#endif /* DNSresolver_hpp */

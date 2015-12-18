//
//  utils.hpp
//  Proxy server
//
//  Created by Kurkin Dmitry on 23.11.15.
//  Copyright © 2015 Kurkin Dmitry. All rights reserved.
//

#ifndef utils_hpp
#define utils_hpp

#include <unordered_map>
#include <list>

template<typename key_t, typename val_t>
struct lru_cache
{
    typedef typename std::pair<key_t, val_t> pair;
    typedef typename std::list<pair>::iterator iterator;
    
    lru_cache(size_t size) : max_size(size) {};
    
    const val_t& get(const key_t& key) {
        auto it = items_map.find(key);
        if (it == items_map.end()) {
            throw std::runtime_error("No such element");
        } else {
            items_list.splice(items_list.begin(), items_list, it->second);
            return it->second->second;
        }
    }
    
    void put(const key_t& key, const val_t& val) {
        auto it = items_map.find(key);
        if (it != items_map.end()) {
            items_list.erase(it->second);
            items_map.erase(it);
        }
        
        items_list.push_front(std::make_pair(key, val));
        items_map[key] = items_list.begin();
        
        if (size() > max_size) {
            auto last = items_list.end()--;
            items_map.erase(last->first);
            items_list.pop_back();
        }
    }
    
    bool contain(const key_t& key) const { return items_map.find(key) != items_map.end(); };
    size_t size() const { return items_map.size(); };
    
private:
    size_t max_size;
    std::list<std::pair<key_t, val_t>> items_list;
    std::unordered_map<key_t, iterator> items_map;
};

#endif /* utils_hpp */

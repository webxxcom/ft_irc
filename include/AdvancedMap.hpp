#pragma once

#include <map>

template <typename Key, typename Value>
class AdvancedMap : public std::map<Key, Value>
{
public:
    std::pair<Value, bool> find(Key const& k) const
    {
        typename std::map<Key, Value>::const_iterator it = std::map<Key, Value>::find(k);
        return (it == this->end()) ?
            std::pair<Value, bool>(NULL, false)
            : std::pair<Value, bool>(it->second, true);
    };
};
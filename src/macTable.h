#ifndef MACTABLE_H
#define MACTABLE_H
#include <unordered_map>
#include <functional>
#include <cstdint>

struct macKey{
    uint16_t vlan_id;
    uint64_t src;

    bool operator==(const macKey& k) const {
        return (vlan_id == k.vlan_id && src == k.src);
    }

};

struct macKeyHash {
    size_t operator()(const macKey& key) const {
        size_t h1 = std::hash<uint64_t>{}(key.src);
        size_t h2 = std::hash<uint16_t>{}(key.vlan_id);

        return h1 ^ (h2 << 1);
    }
};

uint64_t convert_address(uint8_t* addr);

class MacTable{
    //map of ((vlan_id, addr), interface)
public:
    void insert(uint8_t* addr, uint8_t interface, uint16_t vlan_id);
    uint8_t destination(uint8_t* addr, uint16_t vlan_id);

    MacTable();
private:
    std::unordered_map<macKey, uint8_t, macKeyHash> table;
};

#endif
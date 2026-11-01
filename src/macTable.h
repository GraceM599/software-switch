#ifndef MACTABLE_H
#define MACTABLE_H
#include <unordered_map>
class MacTable{
    //map of (addr, interface)
public:
    void insert(uint8_t* addr, uint8_t interface);
    uint8_t destination(uint8_t* addr);
    std::unordered_map<uint64_t, uint8_t> get_table();
    MacTable();
private:
    std::unordered_map<uint64_t, uint8_t> table;
    uint64_t convert_address(uint8_t* addr);
};

#endif
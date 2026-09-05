#ifndef MACTABLE_H
#define MACTABLE_H
#include <unordered_map>
class MacTable{
    //map of (addr, interface)
public:
    void insert(uint8_t* addr, uint8_t interface){
        table[convert_address(addr)] = interface;
    }
    uint8_t destination(uint8_t* addr){
        if (table.count(convert_address(addr)) == 0){
            return -1;
        }
        return table[convert_address(addr)];
    }
    MacTable(){

    }
private:
    std::unordered_map<uint64_t, uint8_t> table;
    uint64_t convert_address(uint8_t* addr){
        //6 bytes -> 8 byte sized number
        uint64_t num = 0;
        for (int i = 0; i<6; ++i){
            num += (static_cast<uint64_t>(addr[i])) << i*8;
        }
        return num;
    }
};

#endif
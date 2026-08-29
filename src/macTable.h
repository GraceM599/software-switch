#ifndef MACTABLE_H
#define MACTABLE_H
#include <unordered_map>
class MacTable{
    //map of (addr, interface)
public:
    void insert(uint8_t* addr, uint8_t interface){
        table.insert({convert_address(addr), interface});
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
        uint64_t num;
        for (int i = 5; i>=0; --i){
            num += addr[i] << i*8;
        }
    }
};

#endif
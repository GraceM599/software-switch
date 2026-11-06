#include "macTable.h"

void MacTable::insert(uint8_t* addr, uint8_t interface, uint16_t vlan_id){
    macKey mk{vlan_id, convert_address(addr)};
    table[mk] = interface;
}

uint8_t MacTable::destination(uint8_t* addr, uint16_t vlan_id){
    macKey mk{vlan_id, convert_address(addr)};
    if (table.count(mk) == 0){
        return -1;
    }
    return table[mk];
}


MacTable::MacTable(){

}

uint64_t convert_address(uint8_t* addr){
        //6 bytes -> 8 byte sized number
        uint64_t num = 0;
        for (int i = 0; i<6; ++i){
            num += (static_cast<uint64_t>(addr[i])) << i*8;
        }
        return num;
    }
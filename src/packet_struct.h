#ifndef PACKET_STRUCT
#define PACKET_STRUCT

#include <cstdint>
struct __attribute__((__packed__)) EthernetHeader {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t eth_type;
};


#endif
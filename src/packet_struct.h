#ifndef PACKET_STRUCT
#define PACKET_STRUCT
#include <vector>
#include <cstdint>
struct __attribute__((__packed__)) EthernetHeader {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t eth_type;
};

struct PacketInfo {
    std::vector<uint8_t> data;
    uint8_t ingress_port;
};

#endif
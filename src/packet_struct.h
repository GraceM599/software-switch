#ifndef PACKET_STRUCT
#define PACKET_STRUCT
#include <vector>
#include <cstdint>
#define TLV_HEADER(type, length) static_cast<uint16_t>((((type) & 0x7F) << 9) | ((length) & 0x01FF))
struct __attribute__((__packed__)) EthernetHeader {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t eth_type;
};

struct PacketInfo {
    std::vector<uint8_t> data;
    uint8_t ingress_port;
};

 struct LldpInfo {
    uint32_t switch_id;
    uint16_t port_id;
    uint16_t ttl;
 };
#endif
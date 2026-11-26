#ifndef PACKET_STRUCT
#define PACKET_STRUCT
#include <vector>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <array>
#include <arpa/inet.h>
#define TLV_HEADER(type, length) static_cast<uint16_t>((((type) & 0x7F) << 9) | ((length) & 0x01FF))

inline void appendU16(std::vector<uint8_t>& buf, uint16_t val) {
    uint16_t net_val = htons(val);
    buf.push_back(static_cast<uint8_t>(net_val & 0xFF));
    buf.push_back(static_cast<uint8_t>((net_val >> 8) & 0xFF));
}
inline void appendU32(std::vector<uint8_t>& buf, uint32_t val) {
    uint32_t net_val = htonl(val);
    buf.push_back(static_cast<uint8_t>(net_val & 0xFF));
    buf.push_back(static_cast<uint8_t>((net_val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((net_val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((net_val >> 24) & 0xFF)); 
}


struct __attribute__((__packed__)) EthernetHeader {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t eth_type;
};

struct __attribute__((__packed__)) ISISEthHeader {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t length;
    uint8_t dsap;
    uint8_t ssap;
    uint8_t control;
};

struct PacketInfo {
    std::vector<uint8_t> data;
    uint8_t ingress_port;
};

 struct LldpInfo {
    uint8_t switch_id[6];
    uint16_t port_id;
    uint16_t ttl;
 };

 struct __attribute__((packed)) ISIS_LSP_Header {
    //common header
    uint8_t irpd;
    uint8_t header_len;
    uint8_t protocol_id;
    uint8_t reserved; //0
    uint8_t type; //bits 7-5 reserved (0)
    uint8_t version;
    uint8_t eco; //0; ignored
    uint8_t user_eco; //0; ignored

    //LSP header
    uint16_t pdu_len;
    uint16_t remaining_lifetime;
    uint8_t lsp_id[8];
    uint32_t seq_num;
    uint16_t checksum;
    uint8_t pahit; // P/ATT/HIPPITY/IS TYPE
 };

struct NeighborEntry {
    uint8_t switch_id[6];
    uint16_t port_id;
    uint32_t cost = 10;
    std::chrono::steady_clock::time_point expires_at;
};

inline std::vector<uint8_t> buildISISframe(uint8_t* switch_id, uint8_t* src_mac, uint32_t seq_num,
                                            std::vector<NeighborEntry>& neighbors, 
                                            std::vector<std::array<uint8_t, 6>>& local_hosts)
{
    std::vector<uint8_t> frame;
    ISIS_LSP_Header lsp_header{};
    frame.reserve(256);

    frame.insert(frame.end(), {0x01, 0x80, 0xC2, 0x00, 0x00, 0x14});
    frame.insert(frame.end(), src_mac, src_mac+6);
    appendU16(frame, 0); //length
    frame.push_back(0xFE); frame.push_back(0xFE); frame.push_back(0x03);

    lsp_header.irpd = 0x83;
    lsp_header.header_len = 27;
    lsp_header.protocol_id = 1;
    lsp_header.reserved = 0;
    lsp_header.type = 18;
    lsp_header.version = 1;
    lsp_header.eco = 0;
    lsp_header.user_eco = 0;

    //LSP fields
    lsp_header.pdu_len = 0; //
    lsp_header.remaining_lifetime = htons(1200);
    memcpy(lsp_header.lsp_id, switch_id, 6);
    lsp_header.seq_num = seq_num;
    lsp_header.checksum = 0; //fletcher_checksum(frame);
    lsp_header.pahit = 0x03;

    frame.insert(frame.end(), (uint8_t*)&lsp_header, (uint8_t*)&lsp_header + sizeof(ISIS_LSP_Header));
    if (!neighbors.empty()) {
        frame.push_back(22);
        frame.push_back(static_cast<uint8_t>(10 * neighbors.size()));

        for (const auto& neighbor : neighbors) {
            frame.insert(frame.end(), neighbor.switch_id, neighbor.switch_id+6);
            frame.push_back(static_cast<uint8_t>((neighbor.cost >> 16) & 0xFF));
            frame.push_back(static_cast<uint8_t>((neighbor.cost >> 8) & 0xFF));
            frame.push_back(static_cast<uint8_t>(neighbor.cost & 0xFF));
            frame.push_back(0x00);
        }
    }

    if (!local_hosts.empty()) {
        frame.push_back(147);
        frame.push_back(static_cast<uint8_t>(6 * local_hosts.size()));

        for (const auto& host : local_hosts) {
            frame.insert(frame.end(), host.begin(), host.end());
        }
    }

    uint16_t isis_pdu_len = static_cast<uint16_t>(frame.size() - 17);
    uint16_t net_eth_len = htons(isis_pdu_len + 3);
    uint16_t net_pdu_len = htons(isis_pdu_len);

    memcpy(&frame, &net_eth_len, 2);
    memcpy(&frame, &net_pdu_len, 2);

    //fletcher_checksum(&frame, isis_pdu_len, 24);

    if (frame.size() < 60) {
        frame.resize(60, 0x00);
    }

    return frame;

}

inline bool parseISISFrame(std::vector<uint8_t>& frame, LldpInfo& ll_info){
    
    return true;

}

inline uint16_t fletcher_checksum(uint8_t* data, size_t len, size_t checksum_offset){
    data[checksum_offset] = 0;
    data[checksum_offset + 1] = 0;

    int32_t c0 = 0;
    int32_t c1 = 0;
    for (size_t i = 12; i < len; ++i) {
        c0 = (c0 + data[i]) % 255;
        c1 = (c1 + c0) % 255;
    }

    int32_t mul = static_cast<int32_t>(len - checksum_offset - 1);
    int32_t x = (mul * c0 - c1) % 255;
    if (x <= 0) x += 255;

    int32_t y = 510 - c0 - x;
    if (y > 255) y -= 255;

    data[checksum_offset] = static_cast<uint8_t>(x);
    data[checksum_offset + 1] = static_cast<uint8_t>(y);

    return (static_cast<uint16_t>(x) << 8) | static_cast<uint16_t>(y);
}
#endif
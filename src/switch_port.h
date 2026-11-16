#ifndef PORT_H
#define PORT_H

#include <atomic>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ring_logger.h"

#include <mutex> 
#include <vector>
#include <unordered_set>


enum class PortMode {
    Access, Trunk
};

class Port {
public:
    std::mutex mtx;
    Port(std::string interface_name, PortMode pm, std::vector<uint16_t> vlan_ids);

    ssize_t get(uint8_t* out_buffer, size_t buffer_size, uint16_t& vlan_id);

    int send(uint8_t* packet, size_t length, uint16_t vlan_id);

    int get_fd() const;

    bool has_pending_packet() const;

    bool is_valid() const;

    inline std::string getInterface(){
        return interface;
    }
    inline int getFd(){
        return fd;
    }
    inline uint16_t get_vlan_id(){
        return vlan_id;
    }
    inline PortMode get_mode(){
        return mode;
    }
    ~Port();

private:
    std::string interface;
    int fd;
    PortMode mode;
    uint16_t vlan_id;
    std::unordered_set<uint16_t> allowed_vlans;
    
    int tx_fd;
    void* rx_ring;
    void* tx_ring;
    tpacket_req3 rx_req{};
    tpacket_req tx_req{};
    int ifindex;
    size_t rx_ring_size;
    size_t tx_ring_size;
    size_t rx_offset_next_pkt = 0;
    unsigned int rx_block_idx = 0;
    unsigned int tx_frame_idx = 0;
    bool valid;
    ringLogger logger;

    std::mutex tx_mutex;

    void fail(const std::string& message);

    void release_rx_block(tpacket_block_desc* block, size_t block_idx);

    bool classify_ingress(uint8_t* pkt, size_t& len, size_t buffer_size, uint16_t& vlan_id);
    bool can_egress(uint16_t vlan_id);
    void release();
};

#endif

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
#include "ring_buffer.h"

#include <mutex> 
class Port {
public:
    std::mutex mtx;
    Port(std::string interface_name);

    ssize_t get(uint8_t* out_buffer, size_t buffer_size);

    int send(uint8_t* packet, size_t length);

    int get_fd() const;

    bool has_pending_packet() const;

    bool is_valid() const;

    ~Port();

private:
    std::string interface;
    int fd;
    
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
    ringBuffer logger;

    void fail(const std::string& message);

    void release_rx_block(tpacket_block_desc* block, size_t block_idx);

    void release();
    //TODO: Vector of semaphores to ensure 1 thread per block at a time.
};

#endif

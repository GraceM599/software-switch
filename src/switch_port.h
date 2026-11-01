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

class Port {
public:
    Port(std::string interface_name);

    ssize_t get(uint8_t* out_buffer, size_t buffer_size);

    int send(uint8_t* packet, size_t length);

    int get_fd();

    bool has_pending_packet() const {
        for (unsigned int i = 0; i < rx_req.tp_block_nr; ++i) {
            size_t block_idx = (rx_block_idx + i) % rx_req.tp_block_nr;
            size_t offset = block_idx * rx_req.tp_block_size;
            auto* block = reinterpret_cast<tpacket_block_desc*>(
                static_cast<char*>(rx_ring) + offset
            );
            if ((block->hdr.bh1.block_status & TP_STATUS_USER) != 0) {
                return true;
            }
        }

        return false;
    }

    bool is_valid();

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

    void fail(const std::string& message) {
        std::cerr << message << ": " << std::strerror(errno) << std::endl;
        release();
    }

    void release_rx_block(tpacket_block_desc* block, size_t block_idx) {
        rx_offset_next_pkt = 0;
        std::atomic_thread_fence(std::memory_order_release);
        block->hdr.bh1.block_status = TP_STATUS_KERNEL;
        rx_block_idx = (block_idx + 1) % rx_req.tp_block_nr;
    }

    void release() {
        if (rx_ring != MAP_FAILED && rx_ring != nullptr) {
            munmap(rx_ring, rx_ring_size);
            rx_ring = MAP_FAILED;
        }
        if (tx_ring != MAP_FAILED && tx_ring != nullptr) {
            munmap(tx_ring, tx_ring_size);
            tx_ring = MAP_FAILED;
        }
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
        if (tx_fd >= 0) {
            close(tx_fd);
            tx_fd = -1;
        }
    }
};

#endif

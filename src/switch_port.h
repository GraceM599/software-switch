#ifndef PORT_H
#define PORT_H
//#include "packet_struct.h"
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>    
#include <linux/if.h>     
#include <linux/if_tun.h> 
#include <cstring> 
#include <cerrno>
#include "ring_buffer.h"
#include <linux/if_packet.h>
#include <netinet/in.h>
#include <sys/mman.h>

class Port{
public:
    Port(){
        //empty
    }
    Port(std::string interface_name){
        interface = interface_name;
        
        unsigned int ifindex = if_nametoindex(interface_name.c_str());
        if (ifindex == 0){
            std::cerr << "could not find interface anme " << interface_name;
        }

        fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

        int v = TPACKET_V3;
        int err = setsockopt(fd, SOL_PACKET, PACKET_VERSION, &v, sizeof(v));
        if (err < 0){
            std::cerr << "Issue setting socket options on port " << interface;
        }

        rx_req.tp_block_size = 4096;
        rx_req.tp_block_nr = 2048;
        rx_req.tp_frame_size = 128;
        rx_req.tp_frame_nr = 256;
        rx_req.tp_retire_blk_tov = 10;
        rx_ring_size = rx_req.tp_block_size * rx_req.tp_block_nr;
        err = setsockopt(fd, SOL_PACKET, PACKET_RX_RING, &rx_req, sizeof(rx_req));
        if (err < 0) {
            close(fd);
            std::cerr << "Could not set TPACKET_V3 on " + interface_name;
        }

        sockaddr_ll socket_address{};
        socket_address.sll_family = AF_PACKET;
        socket_address.sll_protocol = htons(ETH_P_ALL);
        socket_address.sll_ifindex = ifindex;

        err = bind(fd, reinterpret_cast<struct sockaddr*>(&socket_address), sizeof(socket_address));
        if (err < 0){
            close(fd);
            std::cerr << "Could not bind socket to " + interface_name;
        }
        rx_ring = mmap(NULL, rx_req.tp_block_size * rx_req.tp_block_nr, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (rx_ring ==  MAP_FAILED) {
            close(fd);
            std::cerr << "Could not mmap rx ring for " << interface_name;
        }

        tx_req.tp_block_size = 4096;
        tx_req.tp_block_nr = 2048;
        tx_req.tp_frame_size = 128;
        tx_req.tp_frame_nr = 256;
        tx_ring_size = tx_req.tp_block_size * tx_req.tp_block_nr;
        err = setsockopt(fd, SOL_PACKET, PACKET_TX_RING, &tx_req, sizeof(tx_req));
        if (err < 0) {
            close(fd);
            std::cerr << "Could not set TPACKET on " + interface_name;
        }

        tx_ring = mmap(NULL, tx_ring_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (tx_ring ==  MAP_FAILED) {
            close(fd);
            std::cerr << "Could not mmap tx ring for " << interface_name;
        }



    }
    ssize_t get(uint8_t* out_buffer){
        for (int i = 0; i <rx_req.tp_block_nr; ++i){
            size_t offset = rx_block_idx * rx_req.tp_block_size;
            char* ring_start = static_cast<char*>(rx_ring);
            struct tpacket_block_desc* current_block =  reinterpret_cast<tpacket_block_desc*>(ring_start + offset);

            if (current_block->hdr.bh1.block_status == TP_STATUS_USER){
                void* first_pkt = current_block->hdr.bh1.ts_first_pkt;
                memcpy(out_buffer, first_pkt->packet_data, len(data));
            }
        }
        

        


        std::string log = interface + " recieved packet" + "\n";
        logger.addEntry(log); 

        return bytes_read;
    }
    void send(uint8_t* pkt, size_t length){
        std::string log = interface + "sent packet with length " + std::to_string(length) + "\n";
        logger.addEntry(log); 
        
        struct tpacket3_hdr* hdr = NULL;
        for (int i = 0; i< req.tp_frame_nr; i += 1) {
            hdr = (void*)(ring + (req.tp_frame_size * i));
            data = (uint8_t*)hdr + TPACKET_ALIGN(sizeof(struct tpacket3_hdr));

            if (hdr->tp_status == TP_STATUS_AVAILABLE) {
                memcpy(data, pkt_data, pkt_len);
                hdr->tp_len = pkt_len;
                hdr->tp_status = TOP_STATUS_SEND_REQUEST;
            }
        }

        send(std::reinterpret_cast<uint8_t*>(&fd), NULL, 0);


        return;
    }
    int get_fd(){
        return fd;
    }
    ~Port(){
        close(fd);
        munmap(rx_ring, rx_ring_size);
    }
private:
    int fd;
    void* rx_ring; 

    tpacket_req3 rx_req;
    ringBuffer logger;
    std::string interface;
    int ifindex;
    size_t rx_ring_size;
    unsigned int rx_block_idx = 0;

    void* tx_ring;
    tpacket_req tx_req;
    size_t tx_ring_size;

    static void walk_block(struct block_desc *pbd, const int block_num)
    {
        int num_pkts = pbd->h1.num_pkts, i;
        unsigned long bytes = 0;
        struct tpacket3_hdr *ppd;

        ppd = (struct tpacket3_hdr *) ((uint8_t *) pbd +
                                    pbd->h1.offset_to_first_pkt);
        for (i = 0; i < num_pkts; ++i) {
                bytes += ppd->tp_snaplen;
                display(ppd);

                ppd = (struct tpacket3_hdr *) ((uint8_t *) ppd +
                                            ppd->tp_next_offset);
        }

        packets_total += num_pkts;
        bytes_total += bytes;
    }

    static void flush_block(struct block_desc *pbd)
    {
        pbd->h1.block_status = TP_STATUS_KERNEL;
    }
};

#endif